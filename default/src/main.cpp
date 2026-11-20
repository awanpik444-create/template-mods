#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/GameManager.hpp>
#include <Geode/ui/TextInput.hpp>

using namespace geode::prelude;

struct Config {
    static bool getBool(const char* key, bool def = false) {
        return Mod::get()->getSavedValue<bool>(key, def);
    }
    static void setBool(const char* key, bool val) {
        Mod::get()->setSavedValue<bool>(key, val);
    }
    static float getFloat(const char* key, float def = 1.0f) {
        return Mod::get()->getSavedValue<float>(key, def);
    }
    static void setFloat(const char* key, float val) {
        Mod::get()->setSavedValue<float>(key, val);
    }
};

static bool g_menuHidden = false;
static std::vector<StartPosObject*> g_startPosList;
static int g_currentStartPosIdx = -1;

void applySpeedhack() {
    bool enabled = Config::getBool("speed_enabled", false);
    float speed = Config::getFloat("speed_val", 1.0f);
    if (speed < 0.01f) speed = 0.01f;
    if (speed > 100.0f) speed = 100.0f;
    CCDirector::sharedDirector()->getScheduler()->setTimeScale(enabled ? speed : 1.0f);
}

class LuxcoriaMenuPopup : public CCLayerColor {
protected:
    CCScrollLayerExt* m_scroll = nullptr;
    Slider* m_speedSlider = nullptr;
    TextInput* m_speedInput = nullptr;
    std::vector<CCMenuItemSpriteExtra*> m_respawnBtns;

    bool init() override {
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        if (!CCLayerColor::initWithColor({0, 0, 0, 235}, 240.f, winSize.height)) return false;

        this->setPosition({0, 0});
        this->setTouchEnabled(true);
        this->setTouchMode(kCCTouchesOneByOne);

        auto title = CCLabelBMFont::create("LuxcoriaMenu v2.1", "bigFont.fnt");
        title->setScale(0.45f);
        title->setPosition({120.f, winSize.height - 20.f});
        this->addChild(title);

        float scrollHeight = winSize.height - 75.f;
        m_scroll = CCScrollLayerExt::create({0, 0, 240.f, scrollHeight});
        m_scroll->setPosition({0, 35.f});
        m_scroll->setTouchEnabled(true);
        this->addChild(m_scroll);

        auto content = m_scroll->m_contentLayer;
        auto menu = CCMenu::create();
        menu->setPosition({0, 0});
        content->addChild(menu);

        float totalHeight = 520.f;
        content->setContentSize({240.f, totalHeight});
        float y = totalHeight - 20.f;
        float step = 26.f;

        addToggle(menu, content, "Noclip", "noclip", y); y -= step;
        addToggle(menu, content, "Auto Safe Mode", "safe_mode", y, true); y -= step;
        addToggle(menu, content, "StartPos Switcher", "sp_switcher", y, true); y -= step;
        addToggle(menu, content, "Auto LDM & No Shake", "auto_ldm", y, true); y -= step;

        addToggle(menu, content, "SpeedHack", "speed_enabled", y, false, true); y -= step;

        m_speedSlider = Slider::create(this, menu_selector(LuxcoriaMenuPopup::onSliderChanged), 0.75f);
        m_speedSlider->setPosition({120.f, y});
        float curSpeed = Config::getFloat("speed_val", 1.0f);
        float sliderVal = std::clamp((curSpeed - 0.5f) / 0.5f, 0.0f, 1.0f);
        m_speedSlider->setValue(sliderVal);
        content->addChild(m_speedSlider);
        y -= 28.f;
        auto spdLbl = CCLabelBMFont::create("Custom (0.01-100):", "chatFont.fnt");
        spdLbl->setScale(0.6f);
        spdLbl->setAnchorPoint({0.f, 0.5f});
        spdLbl->setPosition({12.f, y});
        content->addChild(spdLbl);

        m_speedInput = TextInput::create(65.f, "1.0", "chatFont.fnt");
        m_speedInput->setScale(0.85f);
        m_speedInput->setPosition({185.f, y});
        m_speedInput->setFilter("0123456789.");
        m_speedInput->setString(fmt::format("{:.2f}", curSpeed));
        m_speedInput->setCallback([this](const std::string& str) {
            if (str.empty()) return;
            float val = std::stof(str);
            val = std::clamp(val, 0.01f, 100.0f);
            Config::setFloat("speed_val", val);
            if (m_speedSlider) {
                m_speedSlider->setValue(std::clamp((val - 0.5f) / 0.5f, 0.0f, 1.0f));
            }
            applySpeedhack();
        });
        content->addChild(m_speedInput);
        y -= step + 4.f;

        addToggle(menu, content, "No Death Effect (Hide Cube)", "no_death", y, true); y -= step;
        addToggle(menu, content, "Disable Trail (Keep Wave/Spider)", "no_trail", y, true); y -= step;
        addToggle(menu, content, "Disable Respawn Flash", "no_flash", y, true); y -= step;
        addToggle(menu, content, "Disable All Particles", "no_particles", y, true); y -= step;

        auto respLbl = CCLabelBMFont::create("Respawn Time:", "chatFont.fnt");
        respLbl->setScale(0.6f);
        respLbl->setAnchorPoint({0.f, 0.5f});
        respLbl->setPosition({12.f, y});
        content->addChild(respLbl);
        y -= 22.f;

        float times[] = {0.1f, 0.2f, 0.3f, 0.5f};
        float curResp = Config::getFloat("respawn_time", 0.5f);
        for (int i = 0; i < 4; i++) {
            auto lbl = CCLabelBMFont::create(fmt::format("{:.1f}s", times[i]).c_str(), "chatFont.fnt");
            lbl->setScale(0.65f);
            lbl->setColor(std::abs(curResp - times[i]) < 0.05f ? ccColor3B{0, 255, 0} : ccColor3B{200, 200, 200});
            auto btn = CCMenuItemSpriteExtra::create(lbl, this, menu_selector(LuxcoriaMenuPopup::onSelectRespawn));
            btn->setPosition({35.f + i * 52.f, y});
            btn->setTag(i);
            menu->addChild(btn);
            m_respawnBtns.push_back(btn);
        }
        y -= step;

        addToggle(menu, content, "Practice Music Hack", "prac_music", y, true); y -= step;
        addToggle(menu, content, "Practice Silver Coins", "prac_coins", y, true); y -= step;
        addToggle(menu, content, "Hide Testmode Text", "hide_testmode", y, true); y -= step;
        addToggle(menu, content, "Copy Level Hack", "copy_hack", y, true); y -= step;
        addToggle(menu, content, "Edit Level Hack", "edit_hack", y, true); y -= step;

        m_scroll->moveToTop();

        auto bottomMenu = CCMenu::create();
        bottomMenu->setPosition({0, 0});
        this->addChild(bottomMenu);

        auto hideLbl = CCLabelBMFont::create("HIDE BTN", "bigFont.fnt");
        hideLbl->setScale(0.35f);
        hideLbl->setColor({255, 180, 100});
        auto hideBtn = CCMenuItemSpriteExtra::create(hideLbl, this, menu_selector(LuxcoriaMenuPopup::onHideBtn));
        hideBtn->setPosition({60.f, 18.f});
        bottomMenu->addChild(hideBtn);

        auto closeLbl = CCLabelBMFont::create("CLOSE", "bigFont.fnt");
        closeLbl->setScale(0.38f);
        auto closeBtn = CCMenuItemSpriteExtra::create(closeLbl, this, menu_selector(LuxcoriaMenuPopup::onClose));
        closeBtn->setPosition({185.f, 18.f});
        bottomMenu->addChild(closeBtn);

        return true;
    }

    void addToggle(CCMenu* menu, CCLayer* parent, const char* title, const char* key, float y, bool def = false, bool isSpeed = false) {
        auto lbl = CCLabelBMFont::create(title, "chatFont.fnt");
        lbl->setScale(0.6f);
        lbl->setAnchorPoint({0.f, 0.5f});
        lbl->setPosition({12.f, y});
        parent->addChild(lbl);

        bool state = Config::getBool(key, def);
        auto dot = CCSprite::createWithSpriteFrameName("GJ_colorBtn_001.png");
        dot->setScale(0.42f);
        dot->setColor(state ? ccColor3B{0, 220, 0} : ccColor3B{220, 0, 0});

        auto btn = CCMenuItemSpriteExtra::create(dot, this, menu_selector(LuxcoriaMenuPopup::onToggle));
        btn->setPosition({218.f, y});
        btn->setID(key);
        btn->setTag(isSpeed ? 1 : 0);
        menu->addChild(btn);
    }

    void onToggle(CCObject* sender) {
        auto btn = static_cast<CCMenuItemSpriteExtra*>(sender);
        std::string key = btn->getID();
        bool newState = !Config::getBool(key.c_str(), false);
        Config::setBool(key.c_str(), newState);

        auto dot = static_cast<CCSprite*>(btn->getNormalImage());
        dot->setColor(newState ? ccColor3B{0, 220, 0} : ccColor3B{220, 0, 0});

        if (btn->getTag() == 1) applySpeedhack();
    }

    void onSliderChanged(CCObject* sender) {
        float val = m_speedSlider->getThumb()->getValue();
        float speed = 0.5f + (val * 0.5f);
        Config::setFloat("speed_val", speed);
        if (m_speedInput) {
            m_speedInput->setString(fmt::format("{:.2f}", speed));
        }
        applySpeedhack();
    }

    void onSelectRespawn(CCObject* sender) {
        float times[] = {0.1f, 0.2f, 0.3f, 0.5f};
        int idx = sender->getTag();
        Config::setFloat("respawn_time", times[idx]);

        for (int i = 0; i < 4; i++) {
            auto lbl = static_cast<CCLabelBMFont*>(m_respawnBtns[i]->getNormalImage());
            lbl->setColor(i == idx ? ccColor3B{0, 255, 0} : ccColor3B{200, 200, 200});
        }
    }

    void onHideBtn(CCObject*) {
        g_menuHidden = true;
        if (auto scene = CCDirector::sharedDirector()->getRunningScene()) {
            if (auto btnMenu = scene->getChildByID("lux-floating-menu")) {
                if (auto btn = btnMenu->getChildByID("lux-float-btn")) {
                    btn->setOpacity(0);
                }
            }
        }
        this->removeFromParentAndCleanup(true);
    }

    void onClose(CCObject*) {
        this->removeFromParentAndCleanup(true);
    }

    bool ccTouchBegan(CCTouch* touch, CCEvent*) override {
        auto pos = touch->getLocation();
        return pos.x <= 240.f;
    }

public:
    static LuxcoriaMenuPopup* create() {
        auto ret = new LuxcoriaMenuPopup();
        if (ret && ret->init()) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }
};
class FloatingButtonManager : public CCObject {
public:
    static void attachToScene(CCScene* scene) {
        if (!scene || scene->getChildByID("lux-floating-menu")) return;

        auto winSize = CCDirector::sharedDirector()->getWinSize();
        auto menu = CCMenu::create();
        menu->setID("lux-floating-menu");
        menu->setPosition({24.f, winSize.height - 24.f});

        auto icon = CCSprite::createWithSpriteFrameName("GJ_deleteIcon_001.png");
        icon->setColor({100, 255, 100});
        icon->setScale(0.85f);

        auto btn = CCMenuItemSpriteExtra::create(icon, menu, menu_selector(FloatingButtonManager::onOpen));
        btn->setID("lux-float-btn");
        btn->setOpacity(g_menuHidden ? 0 : 95);
        menu->addChild(btn);

        scene->addChild(menu, 9999);
    }

    void onOpen(CCObject* sender) {
        auto btn = static_cast<CCMenuItemSpriteExtra*>(sender);
        if (g_menuHidden) {
            g_menuHidden = false;
            btn->setOpacity(95);
            return;
        }
        if (auto scene = CCDirector::sharedDirector()->getRunningScene()) {
            if (!scene->getChildByID("lux-popup")) {
                auto popup = LuxcoriaMenuPopup::create();
                popup->setID("lux-popup");
                scene->addChild(popup, 10000);
            }
        }
    }
};

class $modify(MenuLayer) {
    bool init() override {
        if (!MenuLayer::init()) return false;
        applySpeedhack();
        Loader::get()->queueInMainThread([] {
            FloatingButtonManager::attachToScene(CCDirector::sharedDirector()->getRunningScene());
        });
        return true;
    }
};

class $modify(LuxPlayLayer, PlayLayer) {
    struct Fields {
        CCMenu* spMenu = nullptr;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        g_startPosList.clear();
        g_currentStartPosIdx = -1;

        if (Config::getBool("auto_ldm", true)) {
            level->m_lowDetailModeToggled = true;
            GameManager::sharedState()->setGameVariable("0014", true);
        }

        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        Loader::get()->queueInMainThread([] {
            FloatingButtonManager::attachToScene(CCDirector::sharedDirector()->getRunningScene());
        });

        if (Config::getBool("hide_testmode", true)) {
            for (auto* child : CCArrayExt<CCNode*>(this->getChildren())) {
                if (auto lbl = typeinfo_cast<CCLabelBMFont*>(child)) {
                    if (std::string(lbl->getString()).find("Testmode") != std::string::npos) {
                        lbl->setVisible(false);
                    }
                }
            }
        }

        if (Config::getBool("sp_switcher", true) && !g_startPosList.empty()) {
            auto winSize = CCDirector::sharedDirector()->getWinSize();
            m_fields->spMenu = CCMenu::create();
            m_fields->spMenu->setPosition({winSize.width / 2.f, 22.f});

            auto leftSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
            leftSpr->setScale(0.6f);
            leftSpr->setColor({80, 190, 255});
            leftSpr->setOpacity(130);

            auto rightSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
            rightSpr->setScale(0.6f);
            rightSpr->setFlipX(true);
            rightSpr->setColor({80, 190, 255});
            rightSpr->setOpacity(130);

            auto leftBtn = CCMenuItemSpriteExtra::create(leftSpr, this, menu_selector(LuxPlayLayer::onPrevStartPos));
            leftBtn->setPosition({-45.f, 0.f});

            auto rightBtn = CCMenuItemSpriteExtra::create(rightSpr, this, menu_selector(LuxPlayLayer::onNextStartPos));
            rightBtn->setPosition({45.f, 0.f});

            m_fields->spMenu->addChild(leftBtn);
            m_fields->spMenu->addChild(rightBtn);
            this->addChild(m_fields->spMenu, 100);
        }

        return true;
    }

    void addObject(GameObject* obj) {
        PlayLayer::addObject(obj);
        if (auto sp = typeinfo_cast<StartPosObject*>(obj)) {
            g_startPosList.push_back(sp);
            g_currentStartPosIdx = static_cast<int>(g_startPosList.size()) - 1;
        }
    }

    void togglePracticeMode(bool practice) {
        PlayLayer::togglePracticeMode(practice);
        if (m_fields->spMenu) {
            m_fields->spMenu->setVisible(!practice);
        }
    }

    void onPrevStartPos(CCObject*) {
        switchStartPos(-1);
    }

    void onNextStartPos(CCObject*) {
        switchStartPos(1);
    }

    void switchStartPos(int dir) {
        if (g_startPosList.empty()) return;
        g_currentStartPosIdx += dir;
        if (g_currentStartPosIdx < -1) g_currentStartPosIdx = static_cast<int>(g_startPosList.size()) - 1;
        if (g_currentStartPosIdx >= static_cast<int>(g_startPosList.size())) g_currentStartPosIdx = -1;

        StartPosObject* target = (g_currentStartPosIdx >= 0) ? g_startPosList[g_currentStartPosIdx] : nullptr;
        this->setStartPosObject(target);
        this->resetLevel();
    }

    void destroyPlayer(PlayerObject* player, GameObject* obj) override {
        if (Config::getBool("noclip", false) && obj != m_anticheatSpike) {
            return;
        }
        PlayLayer::destroyPlayer(player, obj);
    }

    void levelComplete() override {
        bool cheated = Config::getBool("noclip", false) ||
                       (Config::getBool("speed_enabled", false) && Config::getFloat("speed_val", 1.0f) < 1.0f);
        if (Config::getBool("safe_mode", true) && cheated) {
            this->resetLevel();
            return;
        }
        PlayLayer::levelComplete();
    }

    void pickupItem(EffectGameObject* obj) {
        if (m_isPracticeMode && Config::getBool("prac_coins", true)) {
            obj->triggerObject(this, 0, nullptr);
            return;
        }
        PlayLayer::pickupItem(obj);
    }
};

class $modify(PlayerObject) {
    void playerDestroyed(bool p0) override {
        if (Config::getBool("no_death", true)) {
            this->setVisible(false);
            return;
        }
        PlayerObject::playerDestroyed(p0);
    }

    void activateStreak() override {
        if (Config::getBool("no_trail", true)) {
            return;
        }
        PlayerObject::activateStreak();
    }
};

class $modify(GJBaseGameLayer) {
    CCParticleSystemQuad* spawnParticle(const char* plist, int zOrder, tCCPositionType posType, CCPoint pos) {
        if (Config::getBool("no_particles", true)) {
            return nullptr;
        }
        return GJBaseGameLayer::spawnParticle(plist, zOrder, posType, pos);
    }
};

class $modify(GameStatsManager) {
    bool isItemUnlocked(UnlockType type, int id) override {
        if (Config::getBool("prac_music", true) && type == UnlockType::GJItem && id == 17) {
            return true;
        }
        return GameStatsManager::isItemUnlocked(type, id);
    }
};

class $modify(LevelInfoLayer) {
    bool init(GJGameLevel* level, bool challenge) override {
        if (Config::getBool("copy_hack", true)) {
            level->m_password = 1;
        }
        return LevelInfoLayer::init(level, challenge);
    }
};

class $modify(PauseLayer) {
    void customSetup() override {
        PauseLayer::customSetup();
        if (Config::getBool("edit_hack", true)) {
            if (auto playLayer = PlayLayer::get()) {
                playLayer->m_level->m_levelType = GJLevelType::Editor;
            }
        }
    }
};
