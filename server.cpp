#include "server.h"

#include <fcitx-utils/event.h>
#include <fcitx-utils/eventloopinterface.h>
#include <fcitx-utils/log.h>
#include <fcitx/event.h>
#include <fcitx/instance.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace fcitx {

FkwhudAddon::FkwhudAddon(Instance *instance) : fcitx5(instance) {
  auto runtime = getenv("XDG_RUNTIME_DIR");
  auto runtimeDir = std::string(runtime ? runtime : "/tmp");
  dataFifoPath = runtimeDir + "/fkwhud-data.pipe";
  ctrlFifoPath = runtimeDir + "/fkwhud-ctrl.pipe";

  if (mkfifo(dataFifoPath.c_str(), 0666) < 0 && errno != EEXIST) {
    FCITX_ERROR() << "Failed to create data pipe: " << strerror(errno);
    return;
  }
  dataFifoFD = -1;

  if (mkfifo(ctrlFifoPath.c_str(), 0666) < 0 && errno != EEXIST) {
    FCITX_ERROR() << "Failed to create control pipe: " << strerror(errno);
    return;
  }
  ctrlFifoFD = open(ctrlFifoPath.c_str(), O_RDONLY | O_NONBLOCK);
  if (ctrlFifoFD < 0) {
    FCITX_ERROR() << "Failed to open control pipe: " << strerror(errno);
    return;
  }

  if (ctrlFifoFD >= 0) {
    controlIO = fcitx5->eventLoop().addIOEvent(ctrlFifoFD, IOEventFlags{IOEventFlag::In},
                                               [this](EventSource *, int, IOEventFlags) {
                                                 handleControlMessage();
                                                 return true;
                                               });
  }

  fcitx5IO = fcitx5->watchEvent(EventType::InputContextKeyEvent, EventWatcherPhase::Default,
                                [this](Event &event) { handleKeypress(event); });

  FCITX_INFO() << "fkwhud addon initialized with dual FIFOs at " << runtimeDir;
}

FkwhudAddon::~FkwhudAddon() {
  fcitx5IO.reset();
  controlIO.reset();

  if (dataFifoFD >= 0)
    close(dataFifoFD);
  if (ctrlFifoFD >= 0)
    close(ctrlFifoFD);

  unlink(dataFifoPath.c_str());
  unlink(ctrlFifoPath.c_str());
}

void FkwhudAddon::handleControlMessage() {
  char msg;
  if (read(ctrlFifoFD, &msg, 1) > 0) {
    if (msg == 'H') {
      clientAlive = true;
      if (dataFifoFD < 0) {
        dataFifoFD = open(dataFifoPath.c_str(), O_WRONLY | O_NONBLOCK);
        if (dataFifoFD < 0)
          FCITX_ERROR() << "Failed to open data pipe: " << strerror(errno);
      }
      FCITX_INFO() << "Client connected";
    } else if (msg == 'G') {
      clientAlive = false;
      FCITX_INFO() << "Client disconnected";
    }
  }
}

void FkwhudAddon::send(const void *data, size_t size) {
  if (!clientAlive || dataFifoFD < 0)
    return;

  if (write(dataFifoFD, data, size) < 0 && errno == EPIPE) {
    clientAlive = false;
    FCITX_INFO() << "Client disconnected (broken pipe)";
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
