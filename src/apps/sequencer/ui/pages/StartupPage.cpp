#include "StartupPage.h"

#include "Config.h"

#include "ui/pages/Pages.h"

#include "model/FileManager.h"

#include "core/fs/FileSystem.h"
#include "core/utils/StringBuilder.h"

#include "os/os.h"

StartupPage::StartupPage(PageManager &manager, PageContext &context) :
    BasePage(manager, context)
{
    _startTicks = os::ticks();
}

void StartupPage::draw(Canvas &canvas) {
    if (_state == State::Initial) {
        _state = State::Loading;
        _engine.suspend();
        FileManager::task([this] () {
#if defined(__EMSCRIPTEN__)
            // Web simulator policy: keep only the built-in demo project.
            for (int slot = 0; slot < 128; ++slot) {
                FixedStringBuilder<32> path;
                path("PROJECTS/%03d.PRO", slot + 1);
                fs::remove(path);
            }
            fs::remove("LAST.DAT");
            return FileManager::writeProject(_model.project(), 0);
#else
            return FileManager::readLastProject(_model.project());
#endif
        }, [this] (fs::Error result) {
            _engine.resume();
            _state = State::Ready;
        });
    }

    if (relTime() > 1.f && _state == State::Ready) {
        close();
    }

    canvas.setColor(Color::None);
    canvas.fill();

    canvas.setColor(Color::Bright);

    canvas.setFont(Font::Small);
    canvas.drawTextCentered(0, 0, Width, 32, "PERFORMER");
    canvas.setFont(Font::Tiny);
    canvas.drawTextCentered(0, 20, Width, 8, "Vinx FW v" CONFIG_VERSION_NAME);

    canvas.setFont(Font::Tiny);
    canvas.drawTextCentered(0, 32, Width, 32, "LOADING ...");

    int w = std::floor(relTime() * Width);
    canvas.fillRect(0, 32 - 2, w, 4);
}

void StartupPage::updateLeds(Leds &leds) {
    leds.clear();

    int progress = std::floor(relTime() * 8.f);
    for (int i = 0; i < 8; ++i) {
        bool active = i <= progress;
        leds.set(MatrixMap::fromTrack(i), active, active);
        leds.set(MatrixMap::fromStep(i), active, active);
        leds.set(MatrixMap::fromStep(i + 8), active, active);
    }
}

float StartupPage::time() const {
    return (os::ticks() - _startTicks) / float(os::time::ms(1000));
}
