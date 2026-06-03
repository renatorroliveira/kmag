// SPDX-FileCopyrightText: 2026 Renato Oliveira <renatorroliveira@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "pipewirecapturemanager.h"

#include "framecrop.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QDBusUnixFileDescriptor>
#include <QEventLoop>
#include <QGuiApplication>
#include <QPointer>
#include <QScreen>
#include <QTimer>
#include <QVariantMap>

#include <pipewire/pipewire.h>
#include <spa/buffer/meta.h>
#include <spa/param/param.h>
#include <spa/param/props.h>
#include <spa/param/video/format-utils.h>

#include <unistd.h>

// Byte size of a cursor-metadata block sized for an embedded bitmap up to WxH.
// KMag never reads the cursor bitmap (cursor_mode = METADATA), but the param
// must advertise a size range the server accepts.
#define KMAG_CURSOR_META_SIZE(w, h) \
    (sizeof(struct spa_meta_cursor) + sizeof(struct spa_meta_bitmap) + (w) * (h) * 4)

// ---- File-scope opaque private state --------------------------------------------

struct PipeWireBackendPrivate {
    pw_thread_loop *loop = nullptr;
    pw_context *ctx = nullptr;
    pw_core *core = nullptr;
    pw_stream *stream = nullptr;
    spa_hook streamHook{};
    spa_video_info_raw fmt{};
    bool haveFormat = false;
};

namespace
{
constexpr char kPortalSvc[] = "org.freedesktop.portal.Desktop";
constexpr char kPortalPath[] = "/org/freedesktop/portal/desktop";
constexpr char kScIface[] = "org.freedesktop.portal.ScreenCast";
constexpr char kReqIface[] = "org.freedesktop.portal.Request";

QImage::Format toQtFormat(std::uint32_t spaFmt)
{
    // On little-endian, QImage::Format_ARGB32 raw bytes are B,G,R,A == SPA BGRA.
    switch (spaFmt) {
    case SPA_VIDEO_FORMAT_BGRA: return QImage::Format_ARGB32;
    case SPA_VIDEO_FORMAT_BGRx: return QImage::Format_RGB32;
    case SPA_VIDEO_FORMAT_RGBA: return QImage::Format_RGBA8888;
    case SPA_VIDEO_FORMAT_RGBx: return QImage::Format_RGBX8888;
    default: return QImage::Format_ARGB32;
    }
}

// ---- PipeWire C callbacks (run on the PipeWire thread) --------------------------

struct CbCtx {
    PipeWireCaptureManager *mgr;
    PipeWireBackendPrivate *impl;
};

void onParamChanged(void *data, std::uint32_t id, const struct spa_pod *param)
{
    auto *ctx = static_cast<CbCtx *>(data);
    if (!param || id != SPA_PARAM_Format) {
        return;
    }
    std::uint32_t mt = 0;
    std::uint32_t mst = 0;
    if (spa_format_parse(param, &mt, &mst) < 0) {
        return;
    }
    if (mt != SPA_MEDIA_TYPE_video || mst != SPA_MEDIA_SUBTYPE_raw) {
        return;
    }
    spa_format_video_raw_parse(param, &ctx->impl->fmt);
    ctx->impl->haveFormat = true;

    // NVIDIA dmabuf workaround: advertise only MemFd|MemPtr so the server uses shm.
    std::uint8_t b[1024];
    struct spa_pod_builder pb = SPA_POD_BUILDER_INIT(b, sizeof(b));
    const struct spa_pod *params[1];
    params[0] = static_cast<const struct spa_pod *>(spa_pod_builder_add_object(&pb,
        SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
        SPA_PARAM_BUFFERS_dataType,
        SPA_POD_CHOICE_FLAGS_Int((1 << SPA_DATA_MemFd) | (1 << SPA_DATA_MemPtr))));
    pw_stream_update_params(ctx->impl->stream, params, 1);
}

void onProcess(void *data)
{
    auto *ctx = static_cast<CbCtx *>(data);
    struct pw_buffer *pb = pw_stream_dequeue_buffer(ctx->impl->stream);
    if (!pb) {
        return;
    }
    struct spa_buffer *buf = pb->buffer;
    struct spa_data *sd = &buf->datas[0];
    struct spa_chunk *c = sd->chunk;

    // Cursor position metadata (cursor_mode = METADATA): present on every buffer
    // while the pointer is on the captured output, in physical stream pixels.
    QPoint cursorPos;
    bool cursorValid = false;
    struct spa_meta_cursor *mc = static_cast<struct spa_meta_cursor *>(
        spa_buffer_find_meta_data(buf, SPA_META_Cursor, sizeof(struct spa_meta_cursor)));
    if (mc && spa_meta_cursor_is_valid(mc)) {
        cursorPos = QPoint(mc->position.x, mc->position.y);
        cursorValid = true;
    }

    if (!ctx->impl->haveFormat || !c || c->size == 0
        || (c->flags & SPA_CHUNK_FLAG_CORRUPTED)) {
        pw_stream_queue_buffer(ctx->impl->stream, pb);
        return;
    }
    if (sd->type == SPA_DATA_DmaBuf || sd->data == nullptr) {
        pw_stream_queue_buffer(ctx->impl->stream, pb); // not CPU-mappable; skip
        return;
    }

    const uchar *pixels = static_cast<const uchar *>(sd->data) + c->offset;
    const int w = static_cast<int>(ctx->impl->fmt.size.width);
    const int h = static_cast<int>(ctx->impl->fmt.size.height);
    const int stride = c->stride > 0 ? c->stride : w * 4;
    QImage img(pixels, w, h, stride, toQtFormat(ctx->impl->fmt.format));
    QImage owned = img.copy();
    pw_stream_queue_buffer(ctx->impl->stream, pb);

    ctx->mgr->publishFrame(owned, cursorPos, cursorValid);
}

void onStateChanged(void *data, enum pw_stream_state, enum pw_stream_state st,
                    const char *err)
{
    auto *ctx = static_cast<CbCtx *>(data);
    if (st == PW_STREAM_STATE_ERROR) {
        ctx->mgr->publishStreamError(QString::fromUtf8(err ? err : "stream error"));
    }
}

const struct pw_stream_events kStreamEvents = {
    .version = PW_VERSION_STREAM_EVENTS,
    .state_changed = onStateChanged,
    .param_changed = onParamChanged,
    .process = onProcess,
};
} // namespace

// ---- Portal handshake relay (QtDBus, ported from the PoC's Portal class) --------

class PortalRelay : public QObject
{
    Q_OBJECT
public:
    PortalRelay(QString restoreToken, QObject *parent = nullptr)
        : QObject(parent)
        , m_bus(QDBusConnection::sessionBus())
        , m_restoreTokenIn(std::move(restoreToken))
    {
    }

    void begin()
    {
        const QString tok = nextToken();
        subscribe(tok, SLOT(onCreate(QDBusMessage)));
        QVariantMap o;
        o.insert(QStringLiteral("handle_token"), tok);
        o.insert(QStringLiteral("session_handle_token"), QStringLiteral("kmagsess"));
        call(QStringLiteral("CreateSession"), {QVariant(o)});
    }

    bool succeeded() const { return m_success; }
    int pwFd() const { return m_pwFd; }
    quint32 nodeId() const { return m_nodeId; }
    QString error() const { return m_error; }
    QString newRestoreToken() const { return m_newToken; }

Q_SIGNALS:
    void finished();

public Q_SLOTS:
    void onCreate(const QDBusMessage &msg)
    {
        QVariantMap res;
        if (!ok(msg, &res, "CreateSession")) {
            return;
        }
        m_session = res.value(QStringLiteral("session_handle")).toString();
        const QString tok = nextToken();
        subscribe(tok, SLOT(onSelect(QDBusMessage)));
        QVariantMap o;
        o.insert(QStringLiteral("handle_token"), tok);
        o.insert(QStringLiteral("types"), uint(1));        // MONITOR
        o.insert(QStringLiteral("multiple"), false);
        o.insert(QStringLiteral("cursor_mode"), uint(4));  // METADATA (position only)
        o.insert(QStringLiteral("persist_mode"), uint(2)); // persist until revoked
        if (!m_restoreTokenIn.isEmpty()) {
            o.insert(QStringLiteral("restore_token"), m_restoreTokenIn);
        }
        call(QStringLiteral("SelectSources"),
             {QVariant(QDBusObjectPath(m_session)), QVariant(o)});
    }

    void onSelect(const QDBusMessage &msg)
    {
        QVariantMap res;
        if (!ok(msg, &res, "SelectSources")) {
            return;
        }
        const QString tok = nextToken();
        subscribe(tok, SLOT(onStart(QDBusMessage)));
        QVariantMap o;
        o.insert(QStringLiteral("handle_token"), tok);
        call(QStringLiteral("Start"),
             {QVariant(QDBusObjectPath(m_session)), QVariant(QString()), QVariant(o)});
    }

    void onStart(const QDBusMessage &msg)
    {
        QVariantMap res;
        if (!ok(msg, &res, "Start")) {
            return;
        }
        m_newToken = res.value(QStringLiteral("restore_token")).toString();

        quint32 nodeId = 0;
        bool got = false;
        const QDBusArgument arg =
            res.value(QStringLiteral("streams")).value<QDBusArgument>();
        arg.beginArray();
        while (!arg.atEnd()) {
            arg.beginStructure();
            uint node = 0;
            QVariantMap props;
            arg >> node >> props;
            arg.endStructure();
            if (!got) {
                nodeId = node;
                got = true;
            }
        }
        arg.endArray();
        if (!got) {
            fail(QStringLiteral("Start returned no streams"));
            return;
        }

        QDBusMessage m = QDBusMessage::createMethodCall(
            QLatin1String(kPortalSvc), QLatin1String(kPortalPath),
            QLatin1String(kScIface), QStringLiteral("OpenPipeWireRemote"));
        m.setArguments({QVariant(QDBusObjectPath(m_session)), QVariant(QVariantMap())});
        QDBusReply<QDBusUnixFileDescriptor> reply = m_bus.call(m);
        if (!reply.isValid()) {
            fail(QStringLiteral("OpenPipeWireRemote: %1").arg(reply.error().message()));
            return;
        }
        m_pwFd = ::dup(reply.value().fileDescriptor());
        m_nodeId = nodeId;
        m_success = true;
        Q_EMIT finished();
    }

private:
    QString nextToken() { return QStringLiteral("kmag%1_%2").arg(::getpid()).arg(m_n++); }

    QString requestPath(const QString &token)
    {
        QString sender = m_bus.baseService();
        if (sender.startsWith(QLatin1Char(':'))) {
            sender.remove(0, 1);
        }
        sender.replace(QLatin1Char('.'), QLatin1Char('_'));
        return QStringLiteral("/org/freedesktop/portal/desktop/request/%1/%2")
            .arg(sender, token);
    }
    void subscribe(const QString &token, const char *slot)
    {
        m_bus.connect(QLatin1String(kPortalSvc), requestPath(token),
                      QLatin1String(kReqIface), QStringLiteral("Response"), this, slot);
    }
    void call(const QString &method, const QList<QVariant> &args)
    {
        QDBusMessage m = QDBusMessage::createMethodCall(
            QLatin1String(kPortalSvc), QLatin1String(kPortalPath),
            QLatin1String(kScIface), method);
        m.setArguments(args);
        m_bus.asyncCall(m);
    }
    bool ok(const QDBusMessage &msg, QVariantMap *out, const char *what)
    {
        const auto a = msg.arguments();
        const uint code = a.value(0).toUInt();
        if (code != 0) {
            fail(QStringLiteral("%1 was denied (code %2)%3")
                     .arg(QLatin1String(what)).arg(code)
                     .arg(code == 1 ? QStringLiteral(" - user cancelled") : QString()));
            return false;
        }
        a.value(1).value<QDBusArgument>() >> *out;
        return true;
    }
    void fail(const QString &why)
    {
        m_error = why;
        m_success = false;
        Q_EMIT finished();
    }

    QDBusConnection m_bus;
    QString m_restoreTokenIn;
    QString m_session;
    QString m_newToken;
    QString m_error;
    int m_pwFd = -1;
    quint32 m_nodeId = 0;
    bool m_success = false;
    int m_n = 0;
};

// ---- Manager ---------------------------------------------------------------------

PipeWireCaptureManager::PipeWireCaptureManager(QObject *parent)
    : ScreenCaptureManager(parent)
    , d(new PipeWireBackendPrivate)
{
}

PipeWireCaptureManager::~PipeWireCaptureManager()
{
    stop();
    delete d;
}

QString PipeWireCaptureManager::backendName() const
{
    return QStringLiteral("Wayland/PipeWire-portal");
}

bool PipeWireCaptureManager::isReady() const
{
    return m_haveFrame.load();
}

bool PipeWireCaptureManager::start()
{
    int pwFd = -1;
    std::uint32_t nodeId = 0;
    if (!runPortalHandshake(&pwFd, &nodeId)) {
        return false; // captureError already emitted inside the handshake
    }
    if (!startPipeWire(pwFd, nodeId)) {
        Q_EMIT captureError(QStringLiteral("Failed to start the PipeWire stream."));
        return false;
    }
    return true;
}

void PipeWireCaptureManager::stop()
{
    if (d && d->loop) {
        pw_thread_loop_lock(d->loop);
        if (d->stream) {
            pw_stream_destroy(d->stream);
            d->stream = nullptr;
        }
        if (d->core) {
            pw_core_disconnect(d->core);
            d->core = nullptr;
        }
        pw_thread_loop_unlock(d->loop);
        pw_thread_loop_stop(d->loop);
        if (d->ctx) {
            pw_context_destroy(d->ctx);
            d->ctx = nullptr;
        }
        pw_thread_loop_destroy(d->loop);
        d->loop = nullptr;
    }

    // The PW thread is now joined (loop destroyed) so no callback can run; safe to
    // take m_mutex without risking the loop-lock/m_mutex ordering. Clear the stale
    // frame so getFrame() returns null after teardown instead of a frozen image.
    QMutexLocker lock(&m_mutex);
    m_haveFrame.store(false);
    m_latestFrame = QImage();
    m_haveCursor = false;
    m_outputGeometry = QRect(); // re-resolve on the next start (outputs may change)
}

void PipeWireCaptureManager::publishFrame(const QImage &fullFrame,
                                          const QPoint &cursorPhysical, bool cursorValid)
{
    QMutexLocker lock(&m_mutex);
    m_latestFrame = fullFrame; // already a deep copy
    m_haveFrame.store(true);
    if (cursorValid) {
        m_cursorPhysical = cursorPhysical;
        m_haveCursor = true;
    }
}

void PipeWireCaptureManager::publishStreamError(const QString &message)
{
    // Drop the stale frame so getFrame() returns null (blank view) after the error.
    {
        QMutexLocker lock(&m_mutex);
        m_haveFrame.store(false);
        m_latestFrame = QImage();
        m_haveCursor = false;
    }
    // Marshal to the GUI thread; captureError() must be emitted there.
    QPointer<PipeWireCaptureManager> self(this);
    QMetaObject::invokeMethod(this, [self, message] {
        if (self) {
            Q_EMIT self->captureError(message);
        }
    }, Qt::QueuedConnection);
}

void PipeWireCaptureManager::resolveOutputGeometryLocked() const
{
    // QScreen::geometry() is in logical pixels but m_latestFrame is physical, so
    // match by PHYSICAL size: geometry().size() * devicePixelRatio(). Matching
    // logical size directly (the previous bug) never matched on a scaled output,
    // fell back to the primary geometry, and then cropped a logical-sized rect
    // out of the physical frame -- confining the magnifier to the top-left
    // 1/scale of the screen.
    if (m_outputGeometry.isValid() || m_latestFrame.isNull()) {
        return;
    }
    const auto screens = QGuiApplication::screens();
    for (QScreen *s : screens) {
        const QSize phys(qRound(s->geometry().width() * s->devicePixelRatio()),
                         qRound(s->geometry().height() * s->devicePixelRatio()));
        if (phys == m_latestFrame.size()) {
            m_outputGeometry = s->geometry();
            break;
        }
    }
    if (!m_outputGeometry.isValid() && !screens.isEmpty()) {
        m_outputGeometry = QGuiApplication::primaryScreen()->geometry();
    }
}

QImage PipeWireCaptureManager::getFrame(const QRect &globalRegion)
{
    QMutexLocker lock(&m_mutex);
    if (!m_haveFrame.load() || m_latestFrame.isNull()) {
        return QImage();
    }

    // Lazily resolve which output this stream is, to map global -> output-local.
    resolveOutputGeometryLocked();

    // Scale-aware crop: maps the logical region to physical pixels and tags the
    // result with the output's device-pixel-ratio (grabWindow parity).
    return KMagCapture::cropGlobalRegion(m_latestFrame, m_outputGeometry, globalRegion);
}

QPoint PipeWireCaptureManager::getCursorPosition() const
{
    QMutexLocker lock(&m_mutex);
    // No cursor metadata yet (stream still starting, or pointer off the captured
    // output): fall back to QCursor::pos(). It is stale on Wayland but harmless
    // as a transient default before the first frame arrives.
    if (!m_haveCursor || m_latestFrame.isNull()) {
        return ScreenCaptureManager::getCursorPosition();
    }
    resolveOutputGeometryLocked();
    if (!m_outputGeometry.isValid()
        || m_outputGeometry.width() <= 0 || m_outputGeometry.height() <= 0) {
        return ScreenCaptureManager::getCursorPosition();
    }
    // Metadata position is in physical stream pixels relative to the captured
    // output. Map to global logical coords with the same per-axis scale the crop
    // uses: globalLogical = outputOrigin + physical / (frameSize / outputSize).
    const qreal sx = qreal(m_latestFrame.width()) / qreal(m_outputGeometry.width());
    const qreal sy = qreal(m_latestFrame.height()) / qreal(m_outputGeometry.height());
    return m_outputGeometry.topLeft()
        + QPoint(qRound(m_cursorPhysical.x() / sx), qRound(m_cursorPhysical.y() / sy));
}

// ---- PipeWire setup --------------------------------------------------------------

bool PipeWireCaptureManager::startPipeWire(int fd, std::uint32_t nodeId)
{
    // pw_context_connect_fd() takes ownership of `fd` only on success. On any
    // failure before/at that call, we must close `fd` ourselves.
    pw_init(nullptr, nullptr);
    d->loop = pw_thread_loop_new("kmag-pw", nullptr);
    if (!d->loop) {
        ::close(fd);
        return false;
    }
    d->ctx = pw_context_new(pw_thread_loop_get_loop(d->loop), nullptr, 0);
    if (!d->ctx) {
        ::close(fd);
        return false;
    }
    if (pw_thread_loop_start(d->loop) < 0) {
        ::close(fd);
        return false;
    }

    // The callback context is intentionally leaked: one per manager, the manager
    // is long-lived, and correctness relies on stop() destroying the
    // pw_thread_loop (which joins the PW thread, so no callback can fire) BEFORE
    // `d` is deleted in the dtor. Freed implicitly at process exit.
    auto *cb = new CbCtx{this, d};

    pw_thread_loop_lock(d->loop);
    d->core = pw_context_connect_fd(d->ctx, fd, nullptr, 0); // takes fd ownership on success
    if (!d->core) {
        pw_thread_loop_unlock(d->loop);
        delete cb;
        ::close(fd); // connect_fd failed -> ownership did not transfer
        return false;
    }

    d->stream = pw_stream_new(d->core, "kmag-capture",
        pw_properties_new(PW_KEY_MEDIA_TYPE, "Video",
                          PW_KEY_MEDIA_CATEGORY, "Capture",
                          PW_KEY_MEDIA_ROLE, "Screen", nullptr));
    if (!d->stream) {
        pw_thread_loop_unlock(d->loop);
        delete cb;
        return false; // connect_fd already owns fd; teardown is left to stop()/dtor
    }
    pw_stream_add_listener(d->stream, &d->streamHook, &kStreamEvents, cb);

    std::uint8_t b[2048];
    struct spa_pod_builder pb = SPA_POD_BUILDER_INIT(b, sizeof(b));
    struct spa_rectangle defSize { 1920, 1080 };
    struct spa_rectangle minSize { 1, 1 };
    struct spa_rectangle maxSize { 8192, 8192 };
    struct spa_fraction defRate { 30, 1 };
    struct spa_fraction minRate { 0, 1 };
    struct spa_fraction maxRate { 360, 1 };
    const struct spa_pod *params[2];
    params[0] = static_cast<const struct spa_pod *>(spa_pod_builder_add_object(&pb,
        SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat,
        SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video),
        SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
        SPA_FORMAT_VIDEO_format,
        SPA_POD_CHOICE_ENUM_Id(4, SPA_VIDEO_FORMAT_BGRA, SPA_VIDEO_FORMAT_BGRA,
                               SPA_VIDEO_FORMAT_RGBA, SPA_VIDEO_FORMAT_BGRx),
        SPA_FORMAT_VIDEO_size,
        SPA_POD_CHOICE_RANGE_Rectangle(&defSize, &minSize, &maxSize),
        SPA_FORMAT_VIDEO_framerate,
        SPA_POD_CHOICE_RANGE_Fraction(&defRate, &minRate, &maxRate)));
    // Request per-frame cursor-position metadata (cursor_mode = METADATA): the
    // Wayland-safe global pointer source for follow-mouse. See getCursorPosition().
    params[1] = static_cast<const struct spa_pod *>(spa_pod_builder_add_object(&pb,
        SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta,
        SPA_PARAM_META_type, SPA_POD_Id(SPA_META_Cursor),
        SPA_PARAM_META_size,
        SPA_POD_CHOICE_RANGE_Int(KMAG_CURSOR_META_SIZE(64, 64),
                                 KMAG_CURSOR_META_SIZE(1, 1),
                                 KMAG_CURSOR_META_SIZE(256, 256))));

    const int r = pw_stream_connect(d->stream, PW_DIRECTION_INPUT, nodeId,
        static_cast<enum pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT
                                          | PW_STREAM_FLAG_MAP_BUFFERS),
        params, 2);
    pw_thread_loop_unlock(d->loop);
    return r >= 0;
}

// ---- Portal handshake (synchronous via nested event loop) ------------------------

bool PipeWireCaptureManager::runPortalHandshake(int *pwFd, std::uint32_t *nodeId)
{
    QEventLoop loop;
    PortalRelay relay(readRestoreToken());
    QObject::connect(&relay, &PortalRelay::finished, &loop, &QEventLoop::quit);
    QTimer::singleShot(90000, &loop, &QEventLoop::quit); // overall safety timeout
    relay.begin();
    loop.exec();

    if (!relay.succeeded()) {
        Q_EMIT captureError(relay.error().isEmpty()
            ? QStringLiteral("Screen-cast portal handshake timed out or was denied. "
                             "KMag needs screen-capture permission on Wayland to function.")
            : relay.error());
        return false;
    }
    const QString tok = relay.newRestoreToken();
    if (!tok.isEmpty()) {
        writeRestoreToken(tok);
    }
    *pwFd = relay.pwFd();
    *nodeId = relay.nodeId();
    return *pwFd >= 0;
}

QString PipeWireCaptureManager::readRestoreToken() const
{
    KConfigGroup cg(KSharedConfig::openConfig(), QStringLiteral("ScreenCapture"));
    return cg.readEntry("RestoreToken", QString());
}

void PipeWireCaptureManager::writeRestoreToken(const QString &token)
{
    KConfigGroup cg(KSharedConfig::openConfig(), QStringLiteral("ScreenCapture"));
    cg.writeEntry("RestoreToken", token);
    cg.sync();
}

#include "pipewirecapturemanager.moc"
