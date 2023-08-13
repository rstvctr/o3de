#include <Editor/PropertyWidgets/BoneMapHandler.h>

#include <AzCore/Memory/SystemAllocator.h>
#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <Editor/PropertyWidgets/ActorJointHandler.h>
#include <EMotionFX/Source/Allocators.h>

#include <QComboBox>
#include <QGraphicsScene>
#include <QGraphicsSvgItem>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace EMotionFX
{
    AZ_CLASS_ALLOCATOR_IMPL(BoneMapWidget, EditorAllocator);
    AZ_CLASS_ALLOCATOR_IMPL(BoneMapHandler, EditorAllocator);

    BoneTargetItem::BoneTargetItem(bool selected, BoneMapState state, AZStd::string name)
        : m_state(state)
        , m_name(name)
        , m_selectedPixmap(":/EMotionFX/BoneMapperHandleSelected.svg")
        , m_unselectedPixmap(":/EMotionFX/BoneMapperHandle.svg")
    {
        QPixmap circle(":/EMotionFX/BoneMapperHandleCircle.svg");
        m_circleMask = circle.createMaskFromColor(QColor(255, 255, 255), Qt::MaskOutColor);
        setToolTip(QString(name.c_str()));
        setFlag(QGraphicsItem::ItemIsSelectable, true);
        setSelected(selected);
    }

    QRectF BoneTargetItem::boundingRect() const
    {
        return m_selectedPixmap.rect();
    }

    void BoneTargetItem::paint(QPainter* painter, [[maybe_unused]] const QStyleOptionGraphicsItem* option, [[maybe_unused]] QWidget* widget)
    {
        QRectF rect = boundingRect();
        if (isSelected())
        {
            painter->drawPixmap(rect, m_selectedPixmap, rect);
        }
        else
        {
            painter->drawPixmap(rect, m_unselectedPixmap, rect);
        }

        // FIXME: These should be customizable
        QColor circleColor;
        switch (m_state)
        {
        case BONE_MAP_STATE_UNSET:
            circleColor = QColor(76, 76, 76);
            break;
        case BONE_MAP_STATE_SET:
            circleColor = QColor(25, 153, 64);
            break;
        case BONE_MAP_STATE_MISSING:
            circleColor = QColor(204, 51, 204);
            break;
        case BONE_MAP_STATE_ERROR:
            circleColor = QColor(204, 51, 51);
            break;
        }
        painter->setPen(circleColor);
        painter->drawPixmap(rect, m_circleMask, rect);
    }

    void BoneTargetItem::SetState(BoneMapState state)
    {
        m_state = state;
        update();
    }

    BoneMapWidget::BoneMapWidget(QWidget* parent)
        : QWidget(parent)
    {
        InitBonesAndGroups();

        // Setup graphics scene
        m_graphicsScene = new QGraphicsScene(this);

        connect(m_graphicsScene, &QGraphicsScene::selectionChanged, this, &BoneMapWidget::onGraphicsSceneSelectionChanged);

        // Add group images and targets
        for (auto& group : m_groups)
        {
            QGraphicsSvgItem* imageItem = new QGraphicsSvgItem(QString(group.m_textureName.c_str()));
            m_graphicsScene->addItem(imageItem);
            imageItem->setVisible(false);
            group.m_imageItem = imageItem;

            for (auto& bone : m_bones)
            {
                if (bone.m_group == group.m_groupName)
                {
                    BoneTargetItem* item = new BoneTargetItem(
                        false,
                        BoneTargetItem::BONE_MAP_STATE_ERROR,
                        bone.m_bone_name);
                    m_graphicsScene->addItem(item);
                    item->setVisible(false);
                    item->setPos((bone.m_handle_offset.GetX() * 256.0) - 6.0, (bone.m_handle_offset.GetY() * 256.0) - 6.0);
                    group.m_boneTargets.push_back(item);
                }
            }
        }

        // Main vertical layout
        QVBoxLayout* mainLayout = new QVBoxLayout(this);

        mainLayout->setMargin(2);
        setLayout(mainLayout);

        QPushButton* guessBoneMappingsButton = new QPushButton("Guess Bone Mapping", this);
        connect(guessBoneMappingsButton, &QAbstractButton::clicked, this, &BoneMapWidget::onGuessBoneMappingsButtonClicked);
        mainLayout->addWidget(guessBoneMappingsButton);

        // Top horizontal layout containing group selection and clear button
        {
            QLabel* groupLabel = new QLabel("Group", this);
            QComboBox* groupSelectComboBox = new QComboBox(this);

            for (const auto& group : m_groups)
            {
                groupSelectComboBox->addItem(QString(group.m_groupName.c_str()));
            }

            connect(groupSelectComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onGroupSelectCurrentIndexChanged(int)));

            groupSelectComboBox->setCurrentIndex(0);
            onGroupSelectCurrentIndexChanged(0);

            QPushButton* clearButton = new QPushButton(QIcon(":/EMotionFX/Clear.svg"), "Clear", this);

            connect(clearButton, &QAbstractButton::clicked, this, &BoneMapWidget::onClearButtonClicked);

            QHBoxLayout* subLayout = new QHBoxLayout(this);
            subLayout->addWidget(groupLabel);
            subLayout->addWidget(groupSelectComboBox, 1);
            subLayout->addWidget(clearButton);

            mainLayout->addLayout(subLayout);
        }

        m_graphicsView = new QGraphicsView(m_graphicsScene, this);
        m_graphicsView->setFixedSize(256, 256);
        m_graphicsView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_graphicsView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_graphicsView->setRenderHint(QPainter::Antialiasing);
        mainLayout->addWidget(m_graphicsView);

        // Bottom horizontal layout for specifying the mapping
        {
            m_targetNameLabel = new QLabel(this);
            m_targetNameLabel->setLineWidth(100);

            m_boneNameLineEdit = new QLineEdit(this);

            m_bonePicker = new ActorJointPicker(true, "Joint Selection Dialog", "Select a joint from the skeleton", this);
            connect(m_bonePicker, &ActorJointPicker::SelectionChanged, this, &BoneMapWidget::onBonePickerSelectionChanged);


            QHBoxLayout* subLayout = new QHBoxLayout(this);
            subLayout->addWidget(m_targetNameLabel);
            subLayout->addWidget(m_boneNameLineEdit, 1);
            subLayout->addWidget(m_bonePicker);

            mainLayout->addLayout(subLayout);
        }

#if 0
        QPushButton* aButton = new QPushButton("a", this);
        layout()->addWidget(aButton);
        QPushButton* bButton = new QPushButton("b", this);
        layout()->addWidget(bButton);

        BoneTargetItem* bt1 = new BoneTargetItem(false, BoneTargetItem::BONE_MAP_STATE_UNSET, "bt1");
        bt1->setPos(10, 10);
        m_graphicsScene->addItem(bt1);
        BoneTargetItem* bt2 = new BoneTargetItem(false, BoneTargetItem::BONE_MAP_STATE_SET, "bt2");
        bt2->setPos(10, 30);
        m_graphicsScene->addItem(bt2);
        BoneTargetItem* bt3 = new BoneTargetItem(false, BoneTargetItem::BONE_MAP_STATE_MISSING, "bt3");
        bt3->setPos(10, 50);
        m_graphicsScene->addItem(bt3);
        BoneTargetItem* bt4 = new BoneTargetItem(false, BoneTargetItem::BONE_MAP_STATE_ERROR, "bt4");
        bt4->setPos(10, 70);
        m_graphicsScene->addItem(bt4);
        BoneTargetItem* bt5 = new BoneTargetItem(true, BoneTargetItem::BONE_MAP_STATE_UNSET, "bt5");
        bt5->setPos(30, 10);
        m_graphicsScene->addItem(bt5);
        BoneTargetItem* bt6 = new BoneTargetItem(true, BoneTargetItem::BONE_MAP_STATE_SET, "bt6");
        bt6->setPos(30, 30);
        m_graphicsScene->addItem(bt6);
        BoneTargetItem* bt7 = new BoneTargetItem(true, BoneTargetItem::BONE_MAP_STATE_MISSING, "bt7");
        bt7->setPos(30, 50);
        m_graphicsScene->addItem(bt7);
        BoneTargetItem* bt8 = new BoneTargetItem(true, BoneTargetItem::BONE_MAP_STATE_ERROR, "bt8");
        bt8->setPos(30, 70);
        m_graphicsScene->addItem(bt8);
#endif
    }

    void BoneMapWidget::onGraphicsSceneSelectionChanged()
    {
        // Unselect current target
        if (m_currentSelectedTarget)
        {
            m_currentSelectedTarget->setSelected(false);
            m_targetNameLabel->setText("");
            m_currentSelectedTarget = nullptr;
        }

        if (m_graphicsScene->selectedItems().size() > 0)
        {
            if (BoneTargetItem* item = dynamic_cast<BoneTargetItem*>(m_graphicsScene->selectedItems()[0]))
            {
                m_targetNameLabel->setText(QString(item->GetName().c_str()));
                // TODO: update bone picker target
                m_currentSelectedTarget = item;
            }
        }
    }

    void BoneMapWidget::onGuessBoneMappingsButtonClicked()
    {
        // TODO: re-run guess of bone mappings
    }

    void BoneMapWidget::onClearButtonClicked()
    {
        // TODO: Clear mappings
    }

    void BoneMapWidget::onGroupSelectCurrentIndexChanged(int index)
    {
        // Set everything invisible
        // FIXME: should probably just set the current group as not visible for efficiency
        for (auto& group : m_groups)
        {
            group.m_imageItem->setVisible(false);

            for (BoneTargetItem* item : group.m_boneTargets)
            {
                item->setVisible(false);
            }
        }

        if (index >= 0 && index < m_groups.size())
        {
            m_groups[index].m_imageItem->setVisible(true);

            for (BoneTargetItem* item : m_groups[index].m_boneTargets)
            {
                item->setVisible(true);
            }
        }
    }

    void BoneMapWidget::onBonePickerSelectionChanged()
    {
        // TODO: update bone mapping
    }

    void BoneMapWidget::InitBonesAndGroups()
    {
        m_groups.resize(4);

        m_groups[0].m_groupName = "Body";
        m_groups[0].m_textureName = ":/EMotionFX/BoneMapHumanBody.svg";
        m_groups[1].m_groupName = "Face";
        m_groups[1].m_textureName = ":/EMotionFX/BoneMapHumanFace.svg";
        m_groups[2].m_groupName = "LeftHand";
        m_groups[2].m_textureName = ":/EMotionFX/BoneMapHumanLeftHand.svg";
        m_groups[3].m_groupName = "RightHand";
        m_groups[3].m_textureName = ":/EMotionFX/BoneMapHumanRightHand.svg";

        m_bones.resize(56);

        m_bones[0].m_bone_name = "Root";
        m_bones[0].m_handle_offset = AZ::Vector2(0.5, 0.91);
        m_bones[0].m_group = "Body";

        m_bones[1].m_bone_name = "Hips";
        m_bones[1].m_bone_parent = "Root";
        m_bones[1].m_tail_direction = TAIL_DIRECTION_SPECIFIC_CHILD;
        m_bones[1].m_bone_tail = "Spine";
        m_bones[1].m_handle_offset = AZ::Vector2(0.5, 0.5);
        m_bones[1].m_group = "Body";
        m_bones[1].m_require = true;

        m_bones[2].m_bone_name = "Spine";
        m_bones[2].m_bone_parent = "Hips";
        m_bones[2].m_handle_offset = AZ::Vector2(0.5, 0.43);
        m_bones[2].m_group = "Body";
        m_bones[2].m_require = true;

        m_bones[3].m_bone_name = "Chest";
        m_bones[3].m_bone_parent = "Spine";
        m_bones[3].m_handle_offset = AZ::Vector2(0.5, 0.36);
        m_bones[3].m_group = "Body";

        m_bones[4].m_bone_name = "UpperChest";
        m_bones[4].m_bone_parent = "Chest";
        m_bones[4].m_handle_offset = AZ::Vector2(0.5, 0.29);
        m_bones[4].m_group = "Body";

        m_bones[5].m_bone_name = "Neck";
        m_bones[5].m_bone_parent = "UpperChest";
        m_bones[5].m_tail_direction = TAIL_DIRECTION_SPECIFIC_CHILD;
        m_bones[5].m_bone_tail = "Head";
        m_bones[5].m_handle_offset = AZ::Vector2(0.5, 0.23);
        m_bones[5].m_group = "Body";
        m_bones[5].m_require = false;

        m_bones[6].m_bone_name = "Head";
        m_bones[6].m_bone_parent = "Neck";
        m_bones[6].m_tail_direction = TAIL_DIRECTION_END;
        m_bones[6].m_handle_offset = AZ::Vector2(0.5, 0.18);
        m_bones[6].m_group = "Body";
        m_bones[6].m_require = true;

        m_bones[7].m_bone_name = "LeftEye";
        m_bones[7].m_bone_parent = "Head";
        m_bones[7].m_handle_offset = AZ::Vector2(0.6, 0.46);
        m_bones[7].m_group = "Face";

        m_bones[8].m_bone_name = "RightEye";
        m_bones[8].m_bone_parent = "Head";
        m_bones[8].m_handle_offset = AZ::Vector2(0.37, 0.46);
        m_bones[8].m_group = "Face";

        m_bones[9].m_bone_name = "Jaw";
        m_bones[9].m_bone_parent = "Head";
        m_bones[9].m_handle_offset = AZ::Vector2(0.46, 0.75);
        m_bones[9].m_group = "Face";

        m_bones[10].m_bone_name = "LeftShoulder";
        m_bones[10].m_bone_parent = "UpperChest";
        m_bones[10].m_handle_offset = AZ::Vector2(0.55, 0.235);
        m_bones[10].m_group = "Body";
        m_bones[10].m_require = true;

        m_bones[11].m_bone_name = "LeftUpperArm";
        m_bones[11].m_bone_parent = "LeftShoulder";
        m_bones[11].m_handle_offset = AZ::Vector2(0.6, 0.24);
        m_bones[11].m_group = "Body";
        m_bones[11].m_require = true;

        m_bones[12].m_bone_name = "LeftLowerArm";
        m_bones[12].m_bone_parent = "LeftUpperArm";
        m_bones[12].m_handle_offset = AZ::Vector2(0.7, 0.24);
        m_bones[12].m_group = "Body";
        m_bones[12].m_require = true;

        m_bones[13].m_bone_name = "LeftHand";
        m_bones[13].m_bone_parent = "LeftLowerArm";
        m_bones[13].m_tail_direction = TAIL_DIRECTION_SPECIFIC_CHILD;
        m_bones[13].m_bone_tail = "LeftMiddleProximal";
        m_bones[13].m_handle_offset = AZ::Vector2(0.82, 0.235);
        m_bones[13].m_group = "Body";
        m_bones[13].m_require = true;

        m_bones[14].m_bone_name = "LeftThumbMetacarpal";
        m_bones[14].m_bone_parent = "LeftHand";
        m_bones[14].m_handle_offset = AZ::Vector2(0.4, 0.8);
        m_bones[14].m_group = "LeftHand";

        m_bones[15].m_bone_name = "LeftThumbProximal";
        m_bones[15].m_bone_parent = "LeftThumbMetacarpal";
        m_bones[15].m_handle_offset = AZ::Vector2(0.3, 0.69);
        m_bones[15].m_group = "LeftHand";

        m_bones[16].m_bone_name = "LeftThumbDistal";
        m_bones[16].m_bone_parent = "LeftThumbProximal";
        m_bones[16].m_handle_offset = AZ::Vector2(0.23, 0.555);
        m_bones[16].m_group = "LeftHand";

        m_bones[17].m_bone_name = "LeftIndexProximal";
        m_bones[17].m_bone_parent = "LeftHand";
        m_bones[17].m_handle_offset = AZ::Vector2(0.413, 0.52);
        m_bones[17].m_group = "LeftHand";

        m_bones[18].m_bone_name = "LeftIndexIntermediate";
        m_bones[18].m_bone_parent = "LeftIndexProximal";
        m_bones[18].m_handle_offset = AZ::Vector2(0.403, 0.36);
        m_bones[18].m_group = "LeftHand";

        m_bones[19].m_bone_name = "LeftIndexDistal";
        m_bones[19].m_bone_parent = "LeftIndexIntermediate";
        m_bones[19].m_handle_offset = AZ::Vector2(0.403, 0.255);
        m_bones[19].m_group = "LeftHand";

        m_bones[20].m_bone_name = "LeftMiddleProximal";
        m_bones[20].m_bone_parent = "LeftHand";
        m_bones[20].m_handle_offset = AZ::Vector2(0.5, 0.51);
        m_bones[20].m_group = "LeftHand";

        m_bones[21].m_bone_name = "LeftMiddleIntermediate";
        m_bones[21].m_bone_parent = "LeftMiddleProximal";
        m_bones[21].m_handle_offset = AZ::Vector2(0.5, 0.345);
        m_bones[21].m_group = "LeftHand";

        m_bones[22].m_bone_name = "LeftMiddleDistal";
        m_bones[22].m_bone_parent = "LeftMiddleIntermediate";
        m_bones[22].m_handle_offset = AZ::Vector2(0.5, 0.22);
        m_bones[22].m_group = "LeftHand";

        m_bones[23].m_bone_name = "LeftRingProximal";
        m_bones[23].m_bone_parent = "LeftHand";
        m_bones[23].m_handle_offset = AZ::Vector2(0.586, 0.52);
        m_bones[23].m_group = "LeftHand";

        m_bones[24].m_bone_name = "LeftRingIntermediate";
        m_bones[24].m_bone_parent = "LeftRingProximal";
        m_bones[24].m_handle_offset = AZ::Vector2(0.59, 0.36);
        m_bones[24].m_group = "LeftHand";

        m_bones[25].m_bone_name = "LeftRingDistal";
        m_bones[25].m_bone_parent = "LeftRingIntermediate";
        m_bones[25].m_handle_offset = AZ::Vector2(0.591, 0.25);
        m_bones[25].m_group = "LeftHand";

        m_bones[26].m_bone_name = "LeftLittleProximal";
        m_bones[26].m_bone_parent = "LeftHand";
        m_bones[26].m_handle_offset = AZ::Vector2(0.663, 0.543);
        m_bones[26].m_group = "LeftHand";

        m_bones[27].m_bone_name = "LeftLittleIntermediate";
        m_bones[27].m_bone_parent = "LeftLittleProximal";
        m_bones[27].m_handle_offset = AZ::Vector2(0.672, 0.415);
        m_bones[27].m_group = "LeftHand";

        m_bones[28].m_bone_name = "LeftLittleDistal";
        m_bones[28].m_bone_parent = "LeftLittleIntermediate";
        m_bones[28].m_handle_offset = AZ::Vector2(0.672, 0.32);
        m_bones[28].m_group = "LeftHand";

        m_bones[29].m_bone_name = "RightShoulder";
        m_bones[29].m_bone_parent = "UpperChest";
        m_bones[29].m_handle_offset = AZ::Vector2(0.45, 0.235);
        m_bones[29].m_group = "Body";
        m_bones[29].m_require = true;

        m_bones[30].m_bone_name = "RightUpperArm";
        m_bones[30].m_bone_parent = "RightShoulder";
        m_bones[30].m_handle_offset = AZ::Vector2(0.4, 0.24);
        m_bones[30].m_group = "Body";
        m_bones[30].m_require = true;

        m_bones[31].m_bone_name = "RightLowerArm";
        m_bones[31].m_bone_parent = "RightUpperArm";
        m_bones[31].m_handle_offset = AZ::Vector2(0.3, 0.24);
        m_bones[31].m_group = "Body";
        m_bones[31].m_require = true;

        m_bones[32].m_bone_name = "RightHand";
        m_bones[32].m_bone_parent = "RightLowerArm";
        m_bones[32].m_tail_direction = TAIL_DIRECTION_SPECIFIC_CHILD;
        m_bones[32].m_bone_tail = "RightMiddleProximal";
        m_bones[32].m_handle_offset = AZ::Vector2(0.18, 0.235);
        m_bones[32].m_group = "Body";
        m_bones[32].m_require = true;

        m_bones[33].m_bone_name = "RightThumbMetacarpal";
        m_bones[33].m_bone_parent = "RightHand";
        m_bones[33].m_handle_offset = AZ::Vector2(0.6, 0.8);
        m_bones[33].m_group = "RightHand";

        m_bones[34].m_bone_name = "RightThumbProximal";
        m_bones[34].m_bone_parent = "RightThumbMetacarpal";
        m_bones[34].m_handle_offset = AZ::Vector2(0.7, 0.69);
        m_bones[34].m_group = "RightHand";

        m_bones[35].m_bone_name = "RightThumbDistal";
        m_bones[35].m_bone_parent = "RightThumbProximal";
        m_bones[35].m_handle_offset = AZ::Vector2(0.77, 0.555);
        m_bones[35].m_group = "RightHand";

        m_bones[36].m_bone_name = "RightIndexProximal";
        m_bones[36].m_bone_parent = "RightHand";
        m_bones[36].m_handle_offset = AZ::Vector2(0.587, 0.52);
        m_bones[36].m_group = "RightHand";

        m_bones[37].m_bone_name = "RightIndexIntermediate";
        m_bones[37].m_bone_parent = "RightIndexProximal";
        m_bones[37].m_handle_offset = AZ::Vector2(0.597, 0.36);
        m_bones[37].m_group = "RightHand";

        m_bones[38].m_bone_name = "RightIndexDistal";
        m_bones[38].m_bone_parent = "RightIndexIntermediate";
        m_bones[38].m_handle_offset = AZ::Vector2(0.597, 0.255);
        m_bones[38].m_group = "RightHand";

        m_bones[39].m_bone_name = "RightMiddleProximal";
        m_bones[39].m_bone_parent = "RightHand";
        m_bones[39].m_handle_offset = AZ::Vector2(0.5, 0.51);
        m_bones[39].m_group = "RightHand";

        m_bones[40].m_bone_name = "RightMiddleIntermediate";
        m_bones[40].m_bone_parent = "RightMiddleProximal";
        m_bones[40].m_handle_offset = AZ::Vector2(0.5, 0.345);
        m_bones[40].m_group = "RightHand";

        m_bones[41].m_bone_name = "RightMiddleDistal";
        m_bones[41].m_bone_parent = "RightMiddleIntermediate";
        m_bones[41].m_handle_offset = AZ::Vector2(0.5, 0.22);
        m_bones[41].m_group = "RightHand";

        m_bones[42].m_bone_name = "RightRingProximal";
        m_bones[42].m_bone_parent = "RightHand";
        m_bones[42].m_handle_offset = AZ::Vector2(0.414, 0.52);
        m_bones[42].m_group = "RightHand";

        m_bones[43].m_bone_name = "RightRingIntermediate";
        m_bones[43].m_bone_parent = "RightRingProximal";
        m_bones[43].m_handle_offset = AZ::Vector2(0.41, 0.36);
        m_bones[43].m_group = "RightHand";

        m_bones[44].m_bone_name = "RightRingDistal";
        m_bones[44].m_bone_parent = "RightRingIntermediate";
        m_bones[44].m_handle_offset = AZ::Vector2(0.409, 0.25);
        m_bones[44].m_group = "RightHand";

        m_bones[45].m_bone_name = "RightLittleProximal";
        m_bones[45].m_bone_parent = "RightHand";
        m_bones[45].m_handle_offset = AZ::Vector2(0.337, 0.543);
        m_bones[45].m_group = "RightHand";

        m_bones[46].m_bone_name = "RightLittleIntermediate";
        m_bones[46].m_bone_parent = "RightLittleProximal";
        m_bones[46].m_handle_offset = AZ::Vector2(0.328, 0.415);
        m_bones[46].m_group = "RightHand";

        m_bones[47].m_bone_name = "RightLittleDistal";
        m_bones[47].m_bone_parent = "RightLittleIntermediate";
        m_bones[47].m_handle_offset = AZ::Vector2(0.328, 0.32);
        m_bones[47].m_group = "RightHand";

        m_bones[48].m_bone_name = "LeftUpperLeg";
        m_bones[48].m_bone_parent = "Hips";
        m_bones[48].m_handle_offset = AZ::Vector2(0.549, 0.49);
        m_bones[48].m_group = "Body";
        m_bones[48].m_require = true;

        m_bones[49].m_bone_name = "LeftLowerLeg";
        m_bones[49].m_bone_parent = "LeftUpperLeg";
        m_bones[49].m_handle_offset = AZ::Vector2(0.548, 0.683);
        m_bones[49].m_group = "Body";
        m_bones[49].m_require = true;

        m_bones[50].m_bone_name = "LeftFoot";
        m_bones[50].m_bone_parent = "LeftLowerLeg";
        m_bones[50].m_handle_offset = AZ::Vector2(0.545, 0.9);
        m_bones[50].m_group = "Body";
        m_bones[50].m_require = true;

        m_bones[51].m_bone_name = "LeftToes";
        m_bones[51].m_bone_parent = "LeftFoot";
        m_bones[51].m_handle_offset = AZ::Vector2(0.545, 0.95);
        m_bones[51].m_group = "Body";

        m_bones[52].m_bone_name = "RightUpperLeg";
        m_bones[52].m_bone_parent = "Hips";
        m_bones[52].m_handle_offset = AZ::Vector2(0.451, 0.49);
        m_bones[52].m_group = "Body";
        m_bones[52].m_require = true;

        m_bones[53].m_bone_name = "RightLowerLeg";
        m_bones[53].m_bone_parent = "RightUpperLeg";
        m_bones[53].m_handle_offset = AZ::Vector2(0.452, 0.683);
        m_bones[53].m_group = "Body";
        m_bones[53].m_require = true;

        m_bones[54].m_bone_name = "RightFoot";
        m_bones[54].m_bone_parent = "RightLowerLeg";
        m_bones[54].m_handle_offset = AZ::Vector2(0.455, 0.9);
        m_bones[54].m_group = "Body";
        m_bones[54].m_require = true;

        m_bones[55].m_bone_name = "RightToes";
        m_bones[55].m_bone_parent = "RightFoot";
        m_bones[55].m_handle_offset = AZ::Vector2(0.455, 0.95);
        m_bones[55].m_group = "Body";
    }

    QWidget* BoneMapHandler::CreateGUI(QWidget* parent)
    {
        BoneMapWidget* widget = aznew BoneMapWidget(parent);

        // TODO: connect signal to know when mappings change

        return widget;
    }

    AZ::u32 BoneMapHandler::GetHandlerName() const
    {
        return AZ_CRC_CE("BoneMapHandler");
    }

    void BoneMapHandler::ConsumeAttribute([[maybe_unused]] BoneMapWidget* widget, [[maybe_unused]] AZ::u32 attrib, [[maybe_unused]] AzToolsFramework::PropertyAttributeReader* attrValue, [[maybe_unused]] const char* debugName)
    {

    }

    void BoneMapHandler::WriteGUIValuesIntoProperty([[maybe_unused]] size_t index, [[maybe_unused]] BoneMapWidget* gui, [[maybe_unused]] property_t& instance, [[maybe_unused]] AzToolsFramework::InstanceDataNode* node)
    {

    }

    bool BoneMapHandler::ReadValuesIntoGUI([[maybe_unused]] size_t index, [[maybe_unused]] BoneMapWidget* gui, [[maybe_unused]] const property_t& instance, [[maybe_unused]] AzToolsFramework::InstanceDataNode* node)
    {
        return true;
    }
}
