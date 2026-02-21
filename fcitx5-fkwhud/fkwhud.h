#ifndef _FCITX5_FKWHUD_H_
#define _FCITX5_FKWHUD_H_

#include "fcitx-utils/eventloopinterface.h"
#include "fcitx/event.h"
#include <cstddef>
#include <fcitx-utils/handlertable.h>
#include <fcitx/addonfactory.h>
#include <fcitx/addoninstance.h>
#include <fcitx/addonmanager.h>
#include <fcitx/instance.h>
#include <memory>
#include <string>

namespace fcitx {

class FkwhudAddon : public AddonInstance {
public:
  FkwhudAddon(Instance *instance);
  ~FkwhudAddon();

private:
  void send(const void *data, size_t size);
  void handleKeypress(Event &event);
  void handleConnect();
  void handleDisconnect();

  Instance *fcitx5;

  std::string sockPath;
  int serverFD = -1;
  int clientFD = -1;
  std::unique_ptr<EventSourceIO> serverIO;
  std::unique_ptr<EventSourceIO> clientIO;
  std::unique_ptr<HandlerTableEntry<EventHandler>> fcitx5IO;
};

class FkwhudAddonFactory : public AddonFactory {
public:
  AddonInstance *create(AddonManager *manager) override {
    return new FkwhudAddon(manager->instance());
  }
};

} // namespace fcitx

#endif
