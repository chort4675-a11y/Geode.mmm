#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>

using namespace geode::prelude;

class DragLayer : public CCLayer {
    CCNode*  m_dragging   = nullptr;
    CCPoint  m_dragOffset = CCPointZero;

    std::vector<CCNode*>                      m_nodes;
    std::unordered_map<CCNode*, CCLayerColor*> m_highlights;

public:
    static DragLayer* create(CCLayer* menuLayer) {
        auto* ret = new DragLayer();
        if (ret && ret->setup(menuLayer)) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }

private:
    bool setup(CCLayer* menuLayer) {
        if (!CCLayer::init()) return false;

        auto ws = CCDirector::get()->getWinSize();
      
        auto* bg = CCLayerColor::create({0, 0, 0, 130}, ws.width, ws.height);
        this->addChild(bg, 0);
      
        for (auto* child : CCArrayExt<CCNode*>(menuLayer->getChildren())) {
            if (auto* menu = typeinfo_cast<CCMenu*>(child)) {
                for (auto* btn : CCArrayExt<CCNode*>(menu->getChildren())) {
                    if (!btn->getID().empty()) {
                        m_nodes.push_back(btn);
                    }
                }
            }
        }

        for (auto* node : m_nodes) {
            auto sz = node->getScaledContentSize();
            auto* hl = CCLayerColor::create(
                {255, 215, 0, 90},
                sz.width + 10.f,
                sz.height + 10.f
            );
            hl->ignoreAnchorPointForPosition(false);
            hl->setAnchorPoint({0.5f, 0.5f});
            this->addChild(hl, 1);
            m_highlights[node] = hl;
        }

        auto* hint = CCLabelBMFont::create(
            "Drag buttons | Done = save | Undo = reset",
            "goldFont.fnt"
        );
        hint->setScale(0.42f);
        hint->setPosition({ws.width / 2.f, ws.height - 14.f});
        this->addChild(hint, 10);

        auto* doneMenu = CCMenu::create();
        auto* doneBtn  = CCMenuItemSpriteExtra::create(
            CCSprite::createWithSpriteFrameName("GJ_doneBtn_001.png"),
            this,
            menu_selector(DragLayer::onDone)
        );
        doneBtn->setScale(0.85f);
        doneMenu->addChild(doneBtn);
        doneMenu->setPosition({ws.width - 32.f, 32.f});
        this->addChild(doneMenu, 10);

        auto* resetMenu = CCMenu::create();
        auto* resetBtn  = CCMenuItemSpriteExtra::create(
            CCSprite::createWithSpriteFrameName("GJ_undoBtn_001.png"),
            this,
            menu_selector(DragLayer::onReset)
        );
        resetBtn->setScale(0.85f);
        resetMenu->addChild(resetBtn);
        resetMenu->setPosition({32.f, 32.f});
        this->addChild(resetMenu, 10);

        this->setTouchEnabled(true);
        this->setTouchMode(kCCTouchesOneByOne);
        this->scheduleUpdate();

        return true;
    }

    CCPoint nodeToLayerSpace(CCNode* node) {
        auto worldPos = node->getParent()->convertToWorldSpace(node->getPosition());
        return this->convertToNodeSpace(worldPos);
    }

    void refreshHighlight(CCNode* node) {
        auto it = m_highlights.find(node);
        if (it == m_highlights.end()) return;
        it->second->setPosition(nodeToLayerSpace(node));
    }


    bool ccTouchBegan(CCTouch* touch, CCEvent*) override {
        auto tp = this->convertTouchToNodeSpace(touch);

        for (auto it = m_nodes.rbegin(); it != m_nodes.rend(); ++it) {
            auto* node = *it;
            auto  np   = nodeToLayerSpace(node);
            auto  sz   = node->getScaledContentSize();

            CCRect r{
                np.x - sz.width  / 2.f,
                np.y - sz.height / 2.f,
                sz.width,
                sz.height
            };

            if (r.containsPoint(tp)) {
                m_dragging   = node;
                m_dragOffset = ccpSub(np, tp);
                return true;
            }
        }
        return true; 
    }

    void ccTouchMoved(CCTouch* touch, CCEvent*) override {
        if (!m_dragging) return;

        auto tp      = this->convertTouchToNodeSpace(touch);
        auto newLP   = ccpAdd(tp, m_dragOffset);           
        auto newWP   = this->convertToWorldSpace(newLP);   
        auto newNode = m_dragging->getParent()
                           ->convertToNodeSpace(newWP);   

        m_dragging->setPosition(newNode);
        refreshHighlight(m_dragging);
    }

    void ccTouchEnded(CCTouch*, CCEvent*) override {
        m_dragging = nullptr;
    }

    
    void update(float) override {
        for (auto* node : m_nodes) refreshHighlight(node);
    }

    

    void onDone(CCObject*) {
        
        auto positions =
            Mod::get()->getSavedValue<std::map<std::string, std::vector<float>>>(
                "button_positions", {});

        
        for (auto* node : m_nodes) {
            auto pos = node->getPosition();
            positions[std::string(node->getID())] = {pos.x, pos.y};
        }

        Mod::get()->setSavedValue("button_positions", positions);

        
        Notification::create("Layout saved!", NotificationIcon::Success)->show();
        this->removeFromParentAndCleanup(true);
    }

    void onReset(CCObject*) {
        
        Mod::get()->setSavedValue(
            "button_positions",
            std::map<std::string, std::vector<float>>{}
        );
      
        CCDirector::get()->replaceScene(MenuLayer::scene(false));
    }
};


class $modify(MyMenuLayer, MenuLayer) {

    bool init() {
        if (!MenuLayer::init()) return false;

        applyPositions(); 
        addEditButton();  

        return true;
    }

    
    void applyPositions() {
        auto positions =
            Mod::get()->getSavedValue<std::map<std::string, std::vector<float>>>(
                "button_positions", {});

        if (positions.empty()) return;

        for (auto* child : CCArrayExt<CCNode*>(this->getChildren())) {
            if (auto* menu = typeinfo_cast<CCMenu*>(child)) {
                for (auto* btn : CCArrayExt<CCNode*>(menu->getChildren())) {
                    auto id = std::string(btn->getID());
                    auto it = positions.find(id);
                    if (it != positions.end() && it->second.size() >= 2) {
                        btn->setPosition({it->second[0], it->second[1]});
                    }
                }
            }
        }
    }

    
    void addEditButton() {
        auto ws = CCDirector::get()->getWinSize();

        auto* spr = CCSprite::createWithSpriteFrameName("GJ_editBtn_001.png");
        if (!spr)
            spr = CCSprite::createWithSpriteFrameName("GJ_optionsBtn_001.png");
        spr->setScale(0.5f);

        auto* btn = CCMenuItemSpriteExtra::create(
            spr, this,
            menu_selector(MyMenuLayer::onOpenEditor)
        );

        auto* menu = CCMenu::create();
        menu->addChild(btn);
        menu->setPosition({22.f, ws.height - 22.f});
        menu->setZOrder(50);
        this->addChild(menu);
    }

  
    void onOpenEditor(CCObject*) {
        if (this->getChildByID("mle-drag-layer"_spr)) return;

        auto* dl = DragLayer::create(this);
        dl->setID("mle-drag-layer"_spr);
        this->addChild(dl, 99);
    }
}
