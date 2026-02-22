#include "server.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <string>

#include "fcitx-utils/event.h"
#include "fcitx-utils/eventloopinterface.h"
#include "fcitx-utils/log.h"
#include "fcitx/addoninstance.h"
#include "fcitx/event.h"
#include "fcitx/instance.h"

// read: socat - $XDG_RUNTIME_DIR/fkwhud.sock

namespace fcitx {

FkwhudAddon::FkwhudAddon(Instance *instance) : fcitx5(instance) {
  sockPath = (getenv("XDG_RUNTIME_DIR") ? std::string(getenv("XDG_RUNTIME_DIR")) : std::string("/tmp")) + "/fkwhud.sock";
  unlink(sockPath.c_str());
  serverFD = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
  if (serverFD < 0) {
    FCITX_ERROR() << "Failed to create socket: " << strerror(errno);
    return;
  }

  sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, sockPath.c_str(), sizeof(addr.sun_path) - 1);

  if (bind(serverFD, (struct sockaddr *)&addr, sizeof(addr)) < 0 || listen(serverFD, 1) < 0) {
    FCITX_ERROR() << "Failed to bind or listen on socket: " << strerror(errno);
    close(serverFD);
    serverFD = -1;
    unlink(sockPath.c_str());
    return;
  }

  fcitx5IO = fcitx5->watchEvent(EventType::InputContextKeyEvent, EventWatcherPhase::Default, [this](Event &event) { handleKeypress(event); });
  serverIO = fcitx5->eventLoop().addIOEvent(serverFD, IOEventFlags{IOEventFlag::In}, [this](EventSource *, int, IOEventFlags) {
    handleConnect();
    return true;
  });

  FCITX_INFO() << "fkwhud addon initialized, socket at " << sockPath;
}

FkwhudAddon::~FkwhudAddon() {
  fcitx5IO.reset();
  serverIO.reset();
  handleDisconnect();

  if (serverFD >= 0) {
    close(serverFD);
    serverFD = -1;
  }

  if (!sockPath.empty()) {
    unlink(sockPath.c_str());
    sockPath.clear();
  }
}

void FkwhudAddon::handleConnect() {
  if (clientFD >= 0) {
    auto fd = accept(serverFD, nullptr, nullptr);
    if (fd >= 0) {
      auto msg = "Another client is already connected.";
      write(fd, msg, strlen(msg) + 1);
      close(fd);
    }
    return;
  }

  clientFD = accept4(serverFD, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
  if (clientFD < 0) {
    FCITX_ERROR() << "Failed to accept client: " << strerror(errno);
    return;
  }
  FCITX_INFO() << "HUD connected";

  clientIO = fcitx5->eventLoop().addIOEvent(clientFD, IOEventFlags{IOEventFlag::Err, IOEventFlag::Hup}, [this](EventSource *, int, IOEventFlags flags) {
    if (flags.test(IOEventFlag::Err) || flags.test(IOEventFlag::Hup)) {
      FCITX_INFO() << "HUD disconnected";
      handleDisconnect();
    }
    return true;
  });
}

void FkwhudAddon::handleDisconnect() {
  clientIO.reset();
  if (clientFD >= 0) {
    close(clientFD);
    clientFD = -1;
  }
}

void FkwhudAddon::send(const void *data, size_t size) {
  if (clientFD < 0)
    return;

  if (write(clientFD, data, size) < 1) {
    if (errno == EPIPE || errno == ECONNRESET) {
      FCITX_INFO() << "HUD disconnected";
      handleDisconnect();
    } else {
      FCITX_ERROR() << "write failed: " << strerror(errno);
    }
  }
}

void FkwhudAddon::handleKeypress(Event &event) {
  auto &keyEvent = static_cast<KeyEvent &>(event);
  auto keysym = keyEvent.key().sym();
  auto keycode = keyEvent.key().code();
  auto pressed = !keyEvent.isRelease();

  send(&(pressed ? "P" : "R"), 1);
  send(&keysym, sizeof(keysym));
  send(&keycode, sizeof(keycode));
}

} // namespace fcitx

FCITX_ADDON_FACTORY(fcitx::FkwhudAddonFactory)
