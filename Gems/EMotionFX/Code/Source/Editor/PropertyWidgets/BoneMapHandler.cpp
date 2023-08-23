/**************************************************************************/
/* The code in this file was adapted from bone_map_editor_plugin.cpp      */
/* in the Godot engine                                                    */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include <Editor/PropertyWidgets/BoneMapHandler.h>

#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/std/sort.h>
#include <AzCore/StringFunc/StringFunc.h>
#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <Editor/PropertyWidgets/ActorJointHandler.h>
#include <EMotionFX/Source/Allocators.h>
#include <SceneAPI/SceneCore/Containers/Scene.h>
#include <SceneAPI/SceneCore/Containers/Views/PairIterator.h>
#include <SceneAPI/SceneCore/DataTypes/GraphData/IBoneData.h>
#include <SceneAPI/SceneCore/DataTypes/IGraphObject.h>
#include <SceneAPI/SceneData/ManifestBase/SceneNodeSelectionList.h>
#include <SceneAPI/SceneUI/CommonWidgets/OverlayWidget.h>
#include <SceneAPI/SceneUI/SceneWidgets/ManifestWidget.h>

#include <QComboBox>
#include <QGraphicsScene>
#include <QGraphicsSvgItem>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
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
                    bone.m_boneTarget = item;
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

            QPushButton* clearButton = new QPushButton("Clear All", this);

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
            m_boneNameLineEdit->setEnabled(false);

            QPushButton* pickButton = new QPushButton(QIcon(":/SceneUI/Manifest/TreeIcon.png"), "", this);
            connect(pickButton, &QAbstractButton::clicked, this, &BoneMapWidget::onPickButtonClicked);

            QPushButton* resetButton = new QPushButton(QIcon(":/EMotionFX/Clear.svg"), "", this);
            connect(resetButton, &QAbstractButton::clicked, this, &BoneMapWidget::onResetButtonClicked);

            QHBoxLayout* subLayout = new QHBoxLayout(this);
            subLayout->addWidget(m_targetNameLabel);
            subLayout->addWidget(m_boneNameLineEdit, 1);
            subLayout->addWidget(pickButton);
            subLayout->addWidget(resetButton);

            mainLayout->addLayout(subLayout);
        }
    }

    const BoneMap& BoneMapWidget::GetBoneMap() const
    {
        return m_boneMapping;
    }

    void BoneMapWidget::SetBoneMap(const BoneMap& boneMap)
    {
        m_boneMapping = boneMap;
        UpdateAllBones(GetGraph());
    }

    void BoneMapWidget::onGraphicsSceneSelectionChanged()
    {
        // Unselect current target
        if (m_currentSelectedTarget)
        {
            m_currentSelectedTarget->setSelected(false);
            m_targetNameLabel->setText("");
            m_boneNameLineEdit->setText("");
            m_currentSelectedTarget = nullptr;
        }

        if (m_graphicsScene->selectedItems().size() > 0)
        {
            if (BoneTargetItem* item = dynamic_cast<BoneTargetItem*>(m_graphicsScene->selectedItems()[0]))
            {
                const AZStd::string& name = item->GetName();
                m_targetNameLabel->setText(QString(name.c_str()));

                if (m_boneMapping.HasBone(name))
                {
                    m_boneNameLineEdit->setText(QString(m_boneMapping.GetOrigBone(name).c_str()));
                }

                m_currentSelectedTarget = item;
            }
        }
    }

    void BoneMapWidget::onGuessBoneMappingsButtonClicked()
    {
        AZ::SceneAPI::Containers::SceneGraph& graph = GetGraph();

        m_boneMapping.Clear();
        GuessBoneMapping(graph, m_boneMapping);
        UpdateAllBones(graph);

        mappingChanged();
    }

    void BoneMapWidget::onClearButtonClicked()
    {
        AZ::SceneAPI::Containers::SceneGraph& graph = GetGraph();

        m_boneMapping.Clear();
        UpdateAllBones(graph);

        mappingChanged();
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

    void BoneMapWidget::onPickButtonClicked()
    {
        AZ_Assert(!m_treeWidget, "Node tree already active, NodeTreeSelectionWidget button pressed multiple times.");
        AZ::SceneAPI::UI::ManifestWidget* root = AZ::SceneAPI::UI::ManifestWidget::FindRoot(this);
        AZ_Assert(root, "NodeTreeSelectionWidget is not a child of a ManifestWidget.");
        if (!root)
        {
            return;
        }

        AZStd::shared_ptr<AZ::SceneAPI::Containers::Scene> scene = root->GetScene();
        if (!scene)
        {
            return;
        }

        AZ::SceneAPI::UI::OverlayWidgetButtonList buttons;
        
        AZ::SceneAPI::UI::OverlayWidgetButton acceptButton;
        acceptButton.m_text = "Select";
        acceptButton.m_callback = AZStd::bind(&BoneMapWidget::OnListChangesAccepted, this);
        acceptButton.m_triggersPop = true;

        AZ::SceneAPI::UI::OverlayWidgetButton cancelButton;
        cancelButton.m_text = "Cancel";
        cancelButton.m_callback = AZStd::bind(&BoneMapWidget::OnListChangesCanceled, this);
        cancelButton.m_triggersPop = true;
        cancelButton.m_isCloseButton = true;

        buttons.push_back(&acceptButton);
        buttons.push_back(&cancelButton);

        // TODO make "list" containing the already selected bone
        AZ::SceneAPI::SceneData::SceneNodeSelectionList list;
        if (m_currentSelectedTarget)
        {
            list.AddSelectedNode(m_boneMapping.GetOrigBone(m_currentSelectedTarget->GetName()));
        }
        m_treeWidget.reset(aznew AZ::SceneAPI::UI::SceneGraphWidget(*scene, list));

        m_treeWidget->SetCheckChildren(false);
        m_treeWidget->AddFilterType(AZ::SceneAPI::DataTypes::IBoneData::TYPEINFO_Uuid());
        m_treeWidget->MakeCheckable(AZ::SceneAPI::UI::SceneGraphWidget::CheckableOption::OnlyFilterTypesCheckable);
        
        m_treeWidget->Build();

        QLabel* label = new QLabel("Finish selecting nodes to continue editing settings.");
        label->setAlignment(Qt::AlignCenter);
        AZ::SceneAPI::UI::OverlayWidget::PushLayerToContainingOverlay(this, label, m_treeWidget.get(), "Select nodes", buttons);
    }

    void BoneMapWidget::onResetButtonClicked()
    {
        if (m_currentSelectedTarget)
        {
            m_boneMapping.Remove(m_currentSelectedTarget->GetName());
            UpdateAllBones(GetGraph()); // FIXME: just update specific bone

            mappingChanged();
        }
    }

    void BoneMapWidget::OnListChangesAccepted()
    {
        std::unique_ptr<AZ::SceneAPI::DataTypes::ISceneNodeSelectionList> list = m_treeWidget->ClaimTargetList();

        if (list->GetSelectedNodeCount() > 0 && m_currentSelectedTarget)
        {
            const AZStd::string& name = m_currentSelectedTarget->GetName(); 
            if (m_boneMapping.HasBone(name))
            {
                m_boneMapping.Remove(name);
            }

            m_boneMapping.SetSkeletonBoneName(name, list->GetSelectedNode(0));
        }

        m_treeWidget.reset();

        UpdateAllBones(GetGraph()); // FIXME: just update specific bone
        mappingChanged();
    }

    void BoneMapWidget::OnListChangesCanceled()
    {
        m_treeWidget.reset();
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

        m_leftWords.push_back(QRegularExpression("(?<![a-zA-Z])left"));
        m_leftWords.push_back(QRegularExpression("(?<![a-zA-Z0-9])l(?![a-zA-Z0-9])"));
        m_rightWords.push_back(QRegularExpression("(?<![a-zA-Z])right"));
        m_rightWords.push_back(QRegularExpression("(?<![a-zA-Z0-9])r(?![a-zA-Z0-9])"));
    }

    QString BoneMapWidget::CamelcaseToUnderscore(const QString& in) {
        const QChar* cstr = in.data();
        QString new_string;
        int start_index = 0;

        for (int i = 1; i < in.size(); i++) {
            bool is_prev_upper = cstr[i - 1].isUpper();
            bool is_prev_lower = cstr[i - 1].isLower();
            bool is_prev_digit = cstr[i - 1].isDigit();

            bool is_curr_upper = cstr[i].isUpper();
            bool is_curr_lower = cstr[i].isLower();
            bool is_curr_digit = cstr[i].isDigit();

            bool is_next_lower = false;
            if (i + 1 < in.size()) {
                is_next_lower = cstr[i + 1].isLower();
            }

            const bool cond_a = is_prev_lower && is_curr_upper; // aA
            const bool cond_b = (is_prev_upper || is_prev_digit) && is_curr_upper && is_next_lower; // AAa, 2Aa
            const bool cond_c = is_prev_digit && is_curr_lower && is_next_lower; // 2aa
            const bool cond_d = (is_prev_upper || is_prev_lower) && is_curr_digit; // A2, a2

            if (cond_a || cond_b || cond_c || cond_d) {
                new_string += in.midRef(start_index, i - start_index) + "_";
                start_index = i;
            }
        }

        new_string += in.midRef(start_index, in.size() - start_index);
        return new_string.toLower();
    }

    QString BoneMapWidget::ToSnakeCase(const QString& in) {
        return CamelcaseToUnderscore(in).replace(QChar(' '), QChar('_')).trimmed(); 
    }

    BoneMapWidget::BoneSegregation BoneMapWidget::GuessBoneSegregation(const QString& boneName) {
        QString fixed_bn = ToSnakeCase(boneName);

        for (int i = 0; i < m_leftWords.size(); i++) {
            if (m_leftWords[i].match(fixed_bn).hasMatch()) {
                return BONE_SEGREGATION_LEFT;
            }
            if (m_rightWords[i].match(fixed_bn).hasMatch()) {
                return BONE_SEGREGATION_RIGHT;
            }
        }

        return BONE_SEGREGATION_NONE;
    }

    AZStd::vector<BoneMapWidget::NodeIndex> BoneMapWidget::GetParentlessBones(const AZ::SceneAPI::Containers::SceneGraph& graph)
    {
        AZStd::vector<NodeIndex> entries;

        auto view = AZ::SceneAPI::Containers::Views::MakePairView(graph.GetNameStorage(), graph.GetContentStorage());
        for (auto it = view.begin(); it != view.end(); ++it)
        {
            if (!it->second || it->first.GetPathLength() == 0)
            {
                continue;
            }

            if (!it->second->RTTI_IsTypeOf(AZ::SceneAPI::DataTypes::IBoneData::TYPEINFO_Uuid()))
            {
                continue;
            }

            NodeIndex node = graph.ConvertToNodeIndex(it.GetFirstIterator());

            if (NodeIndex parent = graph.GetNodeParent(node); parent.IsValid())
            {
                auto content = graph.GetNodeContent(parent);
                if (content && content->RTTI_IsTypeOf(AZ::SceneAPI::DataTypes::IBoneData::TYPEINFO_Uuid()))
                {
                    continue;
                }
            }

            entries.push_back(node);
        }

        return entries;
    }

    AZStd::vector<BoneMapWidget::NodeIndex> BoneMapWidget::GetBoneChildren(const AZ::SceneAPI::Containers::SceneGraph& graph, NodeIndex node)
    {
        AZStd::vector<NodeIndex> entries;

        NodeIndex child = graph.GetNodeChild(node);
        if (child.IsValid())
        {
            entries.push_back(child);

            child = graph.GetNodeSibling(child);
            while (child.IsValid())
            {
                entries.push_back(child);
                child = graph.GetNodeSibling(child);
            }
        }

        return entries;
    }

    BoneMapWidget::NodeIndex BoneMapWidget::GetBoneParent(const AZ::SceneAPI::Containers::SceneGraph& graph, NodeIndex node)
    {
        NodeIndex parent = graph.GetNodeParent(node);

        // type must be bone
        if (parent.IsValid())
        {
            auto content = graph.GetNodeContent(parent);
            if (content && content->RTTI_IsTypeOf(AZ::SceneAPI::DataTypes::IBoneData::TYPEINFO_Uuid()))
            {
                return parent;
            }
        }

        return NodeIndex();
    }

    int BoneMapWidget::CountBones(const AZ::SceneAPI::Containers::SceneGraph& graph, const AZStd::vector<NodeIndex>& boneList)
    {
        int count = 0;
        for (NodeIndex nodeIndex : boneList)
        {
            auto content = graph.GetNodeContent(nodeIndex);
            if (content && content->RTTI_IsTypeOf(AZ::SceneAPI::DataTypes::IBoneData::TYPEINFO_Uuid()))
            {
                count++;
            }
        }

        return count;
    }

    BoneMapWidget::NodeIndex BoneMapWidget::SearchBoneByName(AZ::SceneAPI::Containers::SceneGraph& graph, AZStd::vector<AZStd::string> picklist, BoneSegregation segregation, NodeIndex parent, NodeIndex child, int childrenCount) {
        // There may be multiple candidates hit by existing the subsidiary bone.
        // The one with the shortest name is probably the original.
        QList<AZ::SceneAPI::Containers::SceneGraph::Name> hitList;
        AZ::SceneAPI::Containers::SceneGraph::Name shortest;

        for (int word_idx = 0; word_idx < picklist.size(); word_idx++) {
            QRegularExpression re = QRegularExpression(QString(picklist[word_idx].c_str()));
            if (!child.IsValid()) {
                AZStd::vector<NodeIndex> bonesToProcess = !parent.IsValid() ? GetParentlessBones(graph) : GetBoneChildren(graph, parent);
                while (bonesToProcess.size() > 0) {
                    NodeIndex idx = bonesToProcess[0];
                    bonesToProcess.erase(bonesToProcess.begin());
                    AZStd::vector<NodeIndex> children = GetBoneChildren(graph, idx);
                    for (int i = 0; i < children.size(); i++) {
                        bonesToProcess.push_back(children[i]);
                    }

                    if (childrenCount == 0 && CountBones(graph, children) > 0) {
                        continue;
                    }
                    if (childrenCount > 0 && CountBones(graph, children) < childrenCount) {
                        continue;
                    }

                    QString bn = graph.GetNodeName(idx).GetName();
                    if (re.match(bn.toLower()).hasMatch() && GuessBoneSegregation(bn) == segregation) {
                        hitList.push_back(graph.GetNodeName(idx));
                    }
                }

                if (hitList.size() > 0) {
                    shortest = hitList[0];
                    for (const auto& hit : hitList) {
                        if (hit.GetPathLength() < shortest.GetPathLength()) {
                            shortest = hit; // Prioritize parent.
                        }
                    }
                }
            } else {
                NodeIndex idx = GetBoneParent(graph, child);
                while (idx != parent && idx.IsValid()) {
                    AZStd::vector<NodeIndex> children = GetBoneChildren(graph, idx);
                    if (childrenCount == 0 && CountBones(graph, children) > 0) {
                        continue;
                    }
                    if (childrenCount > 0 && CountBones(graph, children) < childrenCount) {
                        continue;
                    }

                    QString bn = graph.GetNodeName(idx).GetName();
                    if (re.match(bn.toLower()).hasMatch() && GuessBoneSegregation(bn) == segregation) {
                        hitList.push_back(graph.GetNodeName(idx));
                    }
                    idx = GetBoneParent(graph, idx);
                }

                if (hitList.size() > 0) {
                    shortest = hitList[0];
                    for (const auto& hit : hitList) {
                        if (hit.GetPathLength() <= shortest.GetPathLength()) {
                            shortest = hit; // Prioritize parent.
                        }
                    }
                }
            }

            if (shortest.GetPathLength() > 0) {
                break;
            }
        }

        if (shortest.GetPathLength() == 0) {
            return NodeIndex();
        }

        return graph.Find(shortest);
    }

    void BoneMapWidget::GuessBoneMapping(AZ::SceneAPI::Containers::SceneGraph& graph, BoneMap& boneMap)
    {
        AZ_Warning("BoneMapHandler", false, "Run auto mapping.");

        NodeIndex boneIdx;
        AZStd::vector<AZStd::string> picklist; // Use Vector<String> because match words have priority.
        AZStd::vector<NodeIndex> searchPath;

        // 1. Guess Hips
        picklist.push_back("hip");
        picklist.push_back("pelvis");
        picklist.push_back("waist");
        picklist.push_back("torso");
        NodeIndex hips = SearchBoneByName(graph, picklist);
        if (!hips.IsValid()) {
            AZ_Warning("BoneMapHandler", false, "Auto Mapping couldn't guess Hips. Abort auto mapping.");
            return; // If there is no Hips, we cannot guess bone after then.
        } else {
            boneMap.SetSkeletonBoneName("Hips", graph.GetNodeName(hips).GetPath());
        }
        picklist.clear();

        // 2. Guess Root
        boneIdx = GetBoneParent(graph, hips);
        while (boneIdx.IsValid()) {
            searchPath.push_back(boneIdx);
            boneIdx = GetBoneParent(graph, boneIdx);
        }
        if (searchPath.size() == 0) {
            boneIdx = NodeIndex();
        } else if (searchPath.size() == 1) {
            boneIdx = searchPath[0]; // It is only one bone which can be root.
        } else {
            bool found = false;
            for (int i = 0; i < searchPath.size(); i++) {
                QRegularExpression re = QRegularExpression("root");
                if (re.match(QString(graph.GetNodeName(searchPath[i]).GetName()).toLower()).hasMatch()) {
                    boneIdx = searchPath[i]; // Name match is preferred.
                    found = true;
                    break;
                }
            }
            if (!found) {
                for (int i = 0; i < searchPath.size(); i++) {
                    auto boneData = azrtti_cast<AZ::SceneAPI::DataTypes::IBoneData*>(graph.GetNodeContent(searchPath[i]));
                    if (boneData->GetWorldTransform().GetTranslation().IsZero()) {
                        boneIdx = searchPath[i]; // The bone existing at the origin is appropriate as a root.
                        found = true;
                        break;
                    }
                }
            }
            if (!found) {
                boneIdx = searchPath[searchPath.size() - 1]; // Ambiguous, but most parental bone selected.
            }
        }
        if (!boneIdx.IsValid()) {
            AZ_Warning("BoneMapHandler", false, "Auto Mapping couldn't guess Root."); // Root is not required, so continue.
        } else {
            boneMap.SetSkeletonBoneName("Root", graph.GetNodeName(boneIdx).GetPath());
        }
        boneIdx = NodeIndex();
        searchPath.clear();

        // 3. Guess Neck
        picklist.push_back("neck");
        picklist.push_back("head"); // For no neck model.
        picklist.push_back("face"); // Same above.
        NodeIndex neck = SearchBoneByName(graph, picklist, BONE_SEGREGATION_NONE, hips);
        picklist.clear();

        // 4. Guess Head
        picklist.push_back("head");
        picklist.push_back("face");
        NodeIndex head = SearchBoneByName(graph, picklist, BONE_SEGREGATION_NONE, neck);
        if (!head.IsValid()) {
            searchPath = GetBoneChildren(graph, neck);
            if (searchPath.size() == 1) {
                head = searchPath[0]; // Maybe only one child of the Neck is Head.
            }
        }
        if (!head.IsValid()) {
            if (neck.IsValid()) {
                head = neck; // The head animation should have more movement.
                neck = NodeIndex();
                boneMap.SetSkeletonBoneName("Head", graph.GetNodeName(head).GetPath());
            } else {
                AZ_Warning("BoneMapHandler", false, "Auto Mapping couldn't guess Neck or Head."); // Continued for guessing on the other bones. But abort when guessing spines step.
            }
        } else {
            boneMap.SetSkeletonBoneName("Neck", graph.GetNodeName(neck).GetPath());
            boneMap.SetSkeletonBoneName("Head", graph.GetNodeName(head).GetPath());
        }
        picklist.clear();
        searchPath.clear();

        NodeIndex neckOrHead = neck.IsValid() ? neck : (head.IsValid() ? head : NodeIndex());
        if (neckOrHead.IsValid()) {
            // 4-1. Guess Eyes
            picklist.push_back("eye(?!.*(brow|lash|lid))");
            boneIdx = SearchBoneByName(graph, picklist, BONE_SEGREGATION_LEFT, neckOrHead);
            if (!boneIdx.IsValid()) {
                AZ_Warning("BoneMapHandler", false, "Auto Mapping couldn't guess LeftEye.");
            } else {
                boneMap.SetSkeletonBoneName("LeftEye", graph.GetNodeName(boneIdx).GetPath());
            }

            boneIdx = SearchBoneByName(graph, picklist, BONE_SEGREGATION_RIGHT, neckOrHead);
            if (!boneIdx.IsValid()) {
                AZ_Warning("BoneMapHandler", false, "Auto Mapping couldn't guess RightEye.");
            } else {
                boneMap.SetSkeletonBoneName("RightEye", graph.GetNodeName(boneIdx).GetPath());
            }
            picklist.clear();

            // 4-2. Guess Jaw
            picklist.push_back("jaw");
            boneIdx = SearchBoneByName(graph, picklist, BONE_SEGREGATION_NONE, neckOrHead);
            if (!boneIdx.IsValid()) {
                AZ_Warning("BoneMapHandler", false, "Auto Mapping couldn't guess Jaw.");
            } else {
                boneMap.SetSkeletonBoneName("Jaw", graph.GetNodeName(boneIdx).GetPath());
            }
            boneIdx = NodeIndex();
            picklist.clear();
        }

        // 5. Guess Foots
        picklist.push_back("foot");
        picklist.push_back("ankle");
        NodeIndex leftFoot = SearchBoneByName(graph, picklist, BONE_SEGREGATION_LEFT, hips);
        if (!leftFoot.IsValid()) {
            AZ_Warning("BoneMapHandler", false, "Auto Mapping couldn't guess LeftFoot.");
        } else {
            boneMap.SetSkeletonBoneName("LeftFoot", graph.GetNodeName(leftFoot).GetPath());
        }
        NodeIndex rightFoot = SearchBoneByName(graph, picklist, BONE_SEGREGATION_RIGHT, hips);
        if (!rightFoot.IsValid()) {
            AZ_Warning("BoneMapHandler", false, "Auto Mapping couldn't guess RightFoot.");
        } else {
            boneMap.SetSkeletonBoneName("RightFoot", graph.GetNodeName(rightFoot).GetPath());
        }
        picklist.clear();

        // 5-1. Guess LowerLegs
        picklist.push_back("(low|under).*leg");
        picklist.push_back("knee");
        picklist.push_back("shin");
        picklist.push_back("calf");
        picklist.push_back("leg");
        NodeIndex leftLowerLeg;
        if (leftFoot.IsValid()) {
            leftLowerLeg = SearchBoneByName(graph, picklist, BONE_SEGREGATION_LEFT, hips, leftFoot);
        }
        if (!leftLowerLeg.IsValid()) {
            AZ_Warning("BoneMapHandler", false, "Auto Mapping couldn't guess LeftLowerLeg.");
        } else {
            boneMap.SetSkeletonBoneName("LeftLowerLeg", graph.GetNodeName(leftLowerLeg).GetPath());
        }
        NodeIndex rightLowerLeg;
        if (rightFoot.IsValid()) {
            rightLowerLeg = SearchBoneByName(graph, picklist, BONE_SEGREGATION_RIGHT, hips, rightFoot);
        }
        if (!rightLowerLeg.IsValid()) {
            AZ_Warning("BoneMapHandler", false, "Auto Mapping couldn't guess RightLowerLeg.");
        } else {
            boneMap.SetSkeletonBoneName("RightLowerLeg", graph.GetNodeName(rightLowerLeg).GetPath());
        }
        picklist.clear();

        // 5-2. Guess UpperLegs
        picklist.push_back("up.*leg");
        picklist.push_back("thigh");
        picklist.push_back("leg");
        if (leftLowerLeg.IsValid()) {
            boneIdx = SearchBoneByName(graph, picklist, BONE_SEGREGATION_LEFT, hips, leftLowerLeg);
        }
        if (!boneIdx.IsValid()) {
            AZ_Warning("BoneMapHandler", false, "Auto Mapping couldn't guess LeftUpperLeg.");
        } else {
            boneMap.SetSkeletonBoneName("LeftUpperLeg", graph.GetNodeName(boneIdx).GetPath());
        }
        boneIdx = NodeIndex();
        if (rightLowerLeg.IsValid()) {
            boneIdx = SearchBoneByName(graph, picklist, BONE_SEGREGATION_RIGHT, hips, rightLowerLeg);
        }
        if (!boneIdx.IsValid()) {
            AZ_Warning("BoneMapHandler", false, "Auto Mapping couldn't guess RightUpperLeg.");
        } else {
            boneMap.SetSkeletonBoneName("RightUpperLeg", graph.GetNodeName(boneIdx).GetPath());
        }
        boneIdx = NodeIndex();
        picklist.clear();

        // 5-3. Guess Toes
        picklist.push_back("toe");
        picklist.push_back("ball");
        if (leftFoot.IsValid()) {
            boneIdx = SearchBoneByName(graph, picklist, BONE_SEGREGATION_LEFT, leftFoot);
            if (!boneIdx.IsValid()) {
                searchPath = GetBoneChildren(graph, leftFoot);
                if (searchPath.size() == 1) {
                    boneIdx = searchPath[0]; // Maybe only one child of the Foot is Toes.
                }
                searchPath.clear();
            }
        }
        if (!boneIdx.IsValid()) {
            AZ_Warning("BoneMapHandler", false, "Auto Mapping couldn't guess LeftToes.");
        } else {
            boneMap.SetSkeletonBoneName("LeftToes", graph.GetNodeName(boneIdx).GetPath());
        }
        boneIdx = NodeIndex();
        if (rightFoot.IsValid()) {
            boneIdx = SearchBoneByName(graph, picklist, BONE_SEGREGATION_RIGHT, rightFoot);
            if (!boneIdx.IsValid()) {
                searchPath = GetBoneChildren(graph, rightFoot);
                if (searchPath.size() == 1) {
                    boneIdx = searchPath[0]; // Maybe only one child of the Foot is Toes.
                }
                searchPath.clear();
            }
        }
        if (!boneIdx.IsValid()) {
            AZ_Warning("BoneMapHandler", false, "Auto Mapping couldn't guess RightToes.");
        } else {
            boneMap.SetSkeletonBoneName("RightToes", graph.GetNodeName(boneIdx).GetPath());
        }
        boneIdx = NodeIndex();
        picklist.clear();

        // 6. Guess Hands
        picklist.push_back("hand");
        picklist.push_back("wrist");
        picklist.push_back("palm");
        picklist.push_back("fingers");
        NodeIndex leftHandOrPalm = SearchBoneByName(graph, picklist, BONE_SEGREGATION_LEFT, hips, NodeIndex(), 5);
        if (!leftHandOrPalm.IsValid()) {
            // Ambiguous, but try again for fewer finger models.
            leftHandOrPalm = SearchBoneByName(graph, picklist, BONE_SEGREGATION_LEFT, hips);
        }
        NodeIndex leftHand = leftHandOrPalm; // Check for the presence of a wrist, since bones with five children may be palmar.
        while (leftHand.IsValid()) {
            boneIdx = SearchBoneByName(graph, picklist, BONE_SEGREGATION_LEFT, hips, leftHand);
            if (!boneIdx.IsValid()) {
                break;
            }
            leftHand = boneIdx;
        }
        if (!leftHand.IsValid()) {
            AZ_Warning("BoneMapHandler", false, "Auto Mapping couldn't guess LeftHand.");
        } else {
            boneMap.SetSkeletonBoneName("LeftHand", graph.GetNodeName(leftHand).GetPath());
        }
        boneIdx = NodeIndex();
        NodeIndex rightHandOrPalm = SearchBoneByName(graph, picklist, BONE_SEGREGATION_RIGHT, hips, NodeIndex(), 5);
        if (!rightHandOrPalm.IsValid()) {
            // Ambiguous, but try again for fewer finger models.
            rightHandOrPalm = SearchBoneByName(graph, picklist, BONE_SEGREGATION_RIGHT, hips);
        }
        NodeIndex right_hand = rightHandOrPalm;
        while (right_hand.IsValid()) {
            boneIdx = SearchBoneByName(graph, picklist, BONE_SEGREGATION_RIGHT, hips, right_hand);
            if (!boneIdx.IsValid()) {
                break;
            }
            right_hand = boneIdx;
        }
        if (!right_hand.IsValid()) {
            AZ_Warning("BoneMapHandler", false, "Auto Mapping couldn't guess RightHand.");
        } else {
            boneMap.SetSkeletonBoneName("RightHand", graph.GetNodeName(right_hand).GetPath());
        }
        boneIdx = NodeIndex();
        picklist.clear();

        // 6-1. Guess Finger
        bool namedFingerIsFound = false;
        AZStd::vector<AZStd::string> fingers;
        fingers.push_back("thumb|pollex");
        fingers.push_back("index|fore");
        fingers.push_back("middle");
        fingers.push_back("ring");
        fingers.push_back("little|pinkie|pinky");
        if (leftHandOrPalm.IsValid()) {
            AZStd::vector<AZStd::vector<AZStd::string>> leftFingersMap;
            leftFingersMap.resize(5);
            leftFingersMap[0].push_back("LeftThumbMetacarpal");
            leftFingersMap[0].push_back("LeftThumbProximal");
            leftFingersMap[0].push_back("LeftThumbDistal");
            leftFingersMap[1].push_back("LeftIndexProximal");
            leftFingersMap[1].push_back("LeftIndexIntermediate");
            leftFingersMap[1].push_back("LeftIndexDistal");
            leftFingersMap[2].push_back("LeftMiddleProximal");
            leftFingersMap[2].push_back("LeftMiddleIntermediate");
            leftFingersMap[2].push_back("LeftMiddleDistal");
            leftFingersMap[3].push_back("LeftRingProximal");
            leftFingersMap[3].push_back("LeftRingIntermediate");
            leftFingersMap[3].push_back("LeftRingDistal");
            leftFingersMap[4].push_back("LeftLittleProximal");
            leftFingersMap[4].push_back("LeftLittleIntermediate");
            leftFingersMap[4].push_back("LeftLittleDistal");
            for (int i = 0; i < 5; i++) {
                picklist.push_back(fingers[i]);
                NodeIndex finger = SearchBoneByName(graph, picklist, BONE_SEGREGATION_LEFT, leftHandOrPalm, NodeIndex(), 0);
                if (finger.IsValid()) {
                    while (finger != leftHandOrPalm && finger.IsValid()) {
                        searchPath.push_back(finger);
                        finger = GetBoneParent(graph, finger);
                    }
                    AZStd::reverse(searchPath.begin(), searchPath.end());
                    if (searchPath.size() == 1) {
                        boneMap.SetSkeletonBoneName(leftFingersMap[i][0], graph.GetNodeName(searchPath[0]).GetPath());
                        namedFingerIsFound = true;
                    } else if (searchPath.size() == 2) {
                        boneMap.SetSkeletonBoneName(leftFingersMap[i][0], graph.GetNodeName(searchPath[0]).GetPath());
                        boneMap.SetSkeletonBoneName(leftFingersMap[i][1], graph.GetNodeName(searchPath[1]).GetPath());
                        namedFingerIsFound = true;
                    } else if (searchPath.size() >= 3) {
                        // Eliminate the possibility of carpal bone.
                        boneMap.SetSkeletonBoneName(leftFingersMap[i][0], graph.GetNodeName(searchPath[searchPath.size() - 3]).GetPath());
                        boneMap.SetSkeletonBoneName(leftFingersMap[i][1], graph.GetNodeName(searchPath[searchPath.size() - 2]).GetPath());
                        boneMap.SetSkeletonBoneName(leftFingersMap[i][2], graph.GetNodeName(searchPath[searchPath.size() - 1]).GetPath());
                        namedFingerIsFound = true;
                    }
                }
                picklist.clear();
                searchPath.clear();
            }

            // It is a bit corner case, but possibly the finger names are sequentially numbered...
            if (!namedFingerIsFound) {
                picklist.push_back("finger");
                QRegularExpression fingerRe = QRegularExpression("finger");
                searchPath = GetBoneChildren(graph, leftHandOrPalm);
                AZStd::vector<AZStd::string> fingerNames;
                for (int i = 0; i < searchPath.size(); i++) {
                    QString bn = graph.GetNodeName(searchPath[i]).GetName();
                    if (fingerRe.match(bn.toLower()).hasMatch()) {
                        fingerNames.push_back(graph.GetNodeName(searchPath[i]).GetPath());
                    }
                }
                AZStd::sort(fingerNames.begin(), fingerNames.end()); // Order by lexicographic, normal use cases never have more than 10 fingers in one hand.
                searchPath.clear();
                for (int i = 0; i < fingerNames.size(); i++) {
                    if (i >= 5) {
                        break;
                    }
                    NodeIndex finger_root = graph.Find(fingerNames[i]);
                    NodeIndex finger = SearchBoneByName(graph, picklist, BONE_SEGREGATION_LEFT, finger_root, NodeIndex(), 0);
                    if (finger.IsValid()) {
                        while (finger != finger_root && finger.IsValid()) {
                            searchPath.push_back(finger);
                            finger = GetBoneParent(graph, finger);
                        }
                    }
                    searchPath.push_back(finger_root);
                    AZStd::reverse(searchPath.begin(), searchPath.end());
                    if (searchPath.size() == 1) {
                        boneMap.SetSkeletonBoneName(leftFingersMap[i][0], graph.GetNodeName(searchPath[0]).GetPath());
                    } else if (searchPath.size() == 2) {
                        boneMap.SetSkeletonBoneName(leftFingersMap[i][0], graph.GetNodeName(searchPath[0]).GetPath());
                        boneMap.SetSkeletonBoneName(leftFingersMap[i][1], graph.GetNodeName(searchPath[1]).GetPath());
                    } else if (searchPath.size() >= 3) {
                        // Eliminate the possibility of carpal bone.
                        boneMap.SetSkeletonBoneName(leftFingersMap[i][0], graph.GetNodeName(searchPath[searchPath.size() - 3]).GetPath());
                        boneMap.SetSkeletonBoneName(leftFingersMap[i][1], graph.GetNodeName(searchPath[searchPath.size() - 2]).GetPath());
                        boneMap.SetSkeletonBoneName(leftFingersMap[i][2], graph.GetNodeName(searchPath[searchPath.size() - 1]).GetPath());
                    }
                    searchPath.clear();
                }
                picklist.clear();
            }
        }
        namedFingerIsFound = false;
        if (rightHandOrPalm.IsValid()) {
            AZStd::vector<AZStd::vector<AZStd::string>> right_fingers_map;
            right_fingers_map.resize(5);
            right_fingers_map[0].push_back("RightThumbMetacarpal");
            right_fingers_map[0].push_back("RightThumbProximal");
            right_fingers_map[0].push_back("RightThumbDistal");
            right_fingers_map[1].push_back("RightIndexProximal");
            right_fingers_map[1].push_back("RightIndexIntermediate");
            right_fingers_map[1].push_back("RightIndexDistal");
            right_fingers_map[2].push_back("RightMiddleProximal");
            right_fingers_map[2].push_back("RightMiddleIntermediate");
            right_fingers_map[2].push_back("RightMiddleDistal");
            right_fingers_map[3].push_back("RightRingProximal");
            right_fingers_map[3].push_back("RightRingIntermediate");
            right_fingers_map[3].push_back("RightRingDistal");
            right_fingers_map[4].push_back("RightLittleProximal");
            right_fingers_map[4].push_back("RightLittleIntermediate");
            right_fingers_map[4].push_back("RightLittleDistal");
            for (int i = 0; i < 5; i++) {
                picklist.push_back(fingers[i]);
                NodeIndex finger = SearchBoneByName(graph, picklist, BONE_SEGREGATION_RIGHT, rightHandOrPalm, NodeIndex(), 0);
                if (finger.IsValid()) {
                    while (finger != rightHandOrPalm && finger.IsValid()) {
                        searchPath.push_back(finger);
                        finger = GetBoneParent(graph, finger);
                    }
                    AZStd::reverse(searchPath.begin(), searchPath.end());
                    if (searchPath.size() == 1) {
                        boneMap.SetSkeletonBoneName(right_fingers_map[i][0], graph.GetNodeName(searchPath[0]).GetPath());
                        namedFingerIsFound = true;
                    } else if (searchPath.size() == 2) {
                        boneMap.SetSkeletonBoneName(right_fingers_map[i][0], graph.GetNodeName(searchPath[0]).GetPath());
                        boneMap.SetSkeletonBoneName(right_fingers_map[i][1], graph.GetNodeName(searchPath[1]).GetPath());
                        namedFingerIsFound = true;
                    } else if (searchPath.size() >= 3) {
                        boneMap.SetSkeletonBoneName(right_fingers_map[i][0], graph.GetNodeName(searchPath[searchPath.size() - 3]).GetPath());
                        boneMap.SetSkeletonBoneName(right_fingers_map[i][1], graph.GetNodeName(searchPath[searchPath.size() - 2]).GetPath());
                        boneMap.SetSkeletonBoneName(right_fingers_map[i][2], graph.GetNodeName(searchPath[searchPath.size() - 1]).GetPath());
                        namedFingerIsFound = true;
                    }
                }
                picklist.clear();
                searchPath.clear();
            }

            // It is a bit corner case, but possibly the finger names are sequentially numbered...
            if (!namedFingerIsFound) {
                picklist.push_back("finger");
                QRegularExpression fingerRe = QRegularExpression("finger");
                searchPath = GetBoneChildren(graph, rightHandOrPalm);
                AZStd::vector<AZStd::string> fingerNames;
                for (int i = 0; i < searchPath.size(); i++) {
                    QString bn = graph.GetNodeName(searchPath[i]).GetName();
                    if (fingerRe.match(bn.toLower()).hasMatch()) {
                        fingerNames.push_back(graph.GetNodeName(searchPath[i]).GetPath());
                    }
                }
                
                AZStd::sort(fingerNames.begin(), fingerNames.end()); // Order by lexicographic, normal use cases never have more than 10 fingers in one hand.
                searchPath.clear();
                for (int i = 0; i < fingerNames.size(); i++) {
                    if (i >= 5) {
                        break;
                    }
                    NodeIndex finger_root = graph.Find(fingerNames[i]);
                    NodeIndex finger = SearchBoneByName(graph, picklist, BONE_SEGREGATION_RIGHT, finger_root, NodeIndex(), 0);
                    if (finger.IsValid()) {
                        while (finger != finger_root && finger.IsValid()) {
                            searchPath.push_back(finger);
                            finger = GetBoneParent(graph, finger);
                        }
                    }
                    searchPath.push_back(finger_root);
                    AZStd::reverse(searchPath.begin(), searchPath.end());
                    if (searchPath.size() == 1) {
                        boneMap.SetSkeletonBoneName(right_fingers_map[i][0], graph.GetNodeName(searchPath[0]).GetPath());
                    } else if (searchPath.size() == 2) {
                        boneMap.SetSkeletonBoneName(right_fingers_map[i][0], graph.GetNodeName(searchPath[0]).GetPath());
                        boneMap.SetSkeletonBoneName(right_fingers_map[i][1], graph.GetNodeName(searchPath[1]).GetPath());
                    } else if (searchPath.size() >= 3) {
                        boneMap.SetSkeletonBoneName(right_fingers_map[i][0], graph.GetNodeName(searchPath[searchPath.size() - 3]).GetPath());
                        boneMap.SetSkeletonBoneName(right_fingers_map[i][1], graph.GetNodeName(searchPath[searchPath.size() - 2]).GetPath());
                        boneMap.SetSkeletonBoneName(right_fingers_map[i][2], graph.GetNodeName(searchPath[searchPath.size() - 1]).GetPath());
                    }
                    searchPath.clear();
                }
                picklist.clear();
            }
        }

        // 7. Guess Arms
        picklist.push_back("shoulder");
        picklist.push_back("clavicle");
        picklist.push_back("collar");
        NodeIndex leftShoulder = SearchBoneByName(graph, picklist, BONE_SEGREGATION_LEFT, hips);
        if (!leftShoulder.IsValid()) {
            AZ_Warning("BoneMapHandler", false, "Auto Mapping couldn't guess LeftShoulder.");
        } else {
            boneMap.SetSkeletonBoneName("LeftShoulder", graph.GetNodeName(leftShoulder).GetPath());
        }
        NodeIndex rightShoulder = SearchBoneByName(graph, picklist, BONE_SEGREGATION_RIGHT, hips);
        if (!rightShoulder.IsValid()) {
            AZ_Warning("BoneMapHandler", false, "Auto Mapping couldn't guess RightShoulder.");
        } else {
            boneMap.SetSkeletonBoneName("RightShoulder", graph.GetNodeName(rightShoulder).GetPath());
        }
        picklist.clear();

        // 7-1. Guess LowerArms
        picklist.push_back("(low|fore).*arm");
        picklist.push_back("elbow");
        picklist.push_back("arm");
        NodeIndex leftLowerArm;
        if (leftShoulder.IsValid() && leftHandOrPalm.IsValid()) {
            leftLowerArm = SearchBoneByName(graph, picklist, BONE_SEGREGATION_LEFT, leftShoulder, leftHandOrPalm);
        }
        if (!leftLowerArm.IsValid()) {
            AZ_Warning("BoneMapHandler", false, "Auto Mapping couldn't guess LeftLowerArm.");
        } else {
            boneMap.SetSkeletonBoneName("LeftLowerArm", graph.GetNodeName(leftLowerArm).GetPath());
        }
        NodeIndex rightLowerArm;
        if (rightShoulder.IsValid() && rightHandOrPalm.IsValid()) {
            rightLowerArm = SearchBoneByName(graph, picklist, BONE_SEGREGATION_RIGHT, rightShoulder, rightHandOrPalm);
        }
        if (!rightLowerArm.IsValid()) {
            AZ_Warning("BoneMapHandler", false, "Auto Mapping couldn't guess RightLowerArm.");
        } else {
            boneMap.SetSkeletonBoneName("RightLowerArm", graph.GetNodeName(rightLowerArm).GetPath());
        }
        picklist.clear();

        // 7-2. Guess UpperArms
        picklist.push_back("up.*arm");
        picklist.push_back("arm");
        if (leftShoulder.IsValid() && leftLowerArm.IsValid()) {
            boneIdx = SearchBoneByName(graph, picklist, BONE_SEGREGATION_LEFT, leftShoulder, leftLowerArm);
        }
        if (!boneIdx.IsValid()) {
            AZ_Warning("BoneMapHandler", false, "Auto Mapping couldn't guess LeftUpperArm.");
        } else {
            boneMap.SetSkeletonBoneName("LeftUpperArm", graph.GetNodeName(boneIdx).GetPath());
        }
        boneIdx = NodeIndex();
        if (rightShoulder.IsValid() && rightLowerArm.IsValid()) {
            boneIdx = SearchBoneByName(graph, picklist, BONE_SEGREGATION_RIGHT, rightShoulder, rightLowerArm);
        }
        if (!boneIdx.IsValid()) {
            AZ_Warning("BoneMapHandler", false, "Auto Mapping couldn't guess RightUpperArm.");
        } else {
            boneMap.SetSkeletonBoneName("RightUpperArm", graph.GetNodeName(boneIdx).GetPath());
        }
        boneIdx = NodeIndex();
        picklist.clear();

        // 8. Guess UpperChest or Chest
        if (!neckOrHead.IsValid()) {
            return; // Abort.
        }
        NodeIndex chestOrUpperChest = GetBoneParent(graph, neckOrHead);
        bool is_appropriate = true;
        if (leftShoulder.IsValid()) {
            boneIdx = GetBoneParent(graph, leftShoulder);
            bool detect = false;
            while (boneIdx != hips && boneIdx.IsValid()) {
                if (boneIdx == chestOrUpperChest) {
                    detect = true;
                    break;
                }
                boneIdx = GetBoneParent(graph, boneIdx);
            }
            if (!detect) {
                is_appropriate = false;
            }
            boneIdx = NodeIndex();
        }
        if (rightShoulder.IsValid()) {
            boneIdx = GetBoneParent(graph, rightShoulder);
            bool detect = false;
            while (boneIdx != hips && boneIdx.IsValid()) {
                if (boneIdx == chestOrUpperChest) {
                    detect = true;
                    break;
                }
                boneIdx = GetBoneParent(graph, boneIdx);
            }
            if (!detect) {
                is_appropriate = false;
            }
            boneIdx = NodeIndex();
        }
        if (!is_appropriate) {
            if (GetBoneParent(graph, leftShoulder) == GetBoneParent(graph, rightShoulder)) {
                chestOrUpperChest = GetBoneParent(graph, leftShoulder);
            } else {
                chestOrUpperChest = NodeIndex();
            }
        }
        if (!chestOrUpperChest.IsValid()) {
            AZ_Warning("BoneMapHandler", false, "Auto Mapping couldn't guess Chest or UpperChest. Abort auto mapping.");
            return; // Will be not able to guess Spines.
        }

        // 9. Guess Spines
        boneIdx = GetBoneParent(graph, chestOrUpperChest);
        while (boneIdx != hips && boneIdx.IsValid()) {
            searchPath.push_back(boneIdx);
            boneIdx = GetBoneParent(graph, boneIdx);
        }
        AZStd::reverse(searchPath.begin(), searchPath.end());
        if (searchPath.size() == 0) {
            boneMap.SetSkeletonBoneName("Spine", graph.GetNodeName(chestOrUpperChest).GetPath()); // Maybe chibi model...?
        } else if (searchPath.size() == 1) {
            boneMap.SetSkeletonBoneName("Spine", graph.GetNodeName(searchPath[0]).GetPath());
            boneMap.SetSkeletonBoneName("Chest", graph.GetNodeName(chestOrUpperChest).GetPath());
        } else if (searchPath.size() >= 2) {
            boneMap.SetSkeletonBoneName("Spine", graph.GetNodeName(searchPath[0]).GetPath());
            boneMap.SetSkeletonBoneName("Chest", graph.GetNodeName(searchPath[searchPath.size() - 1]).GetPath()); // Probably UppeChest's parent is appropriate.
            boneMap.SetSkeletonBoneName("UpperChest", graph.GetNodeName(chestOrUpperChest).GetPath());
        }
        boneIdx = NodeIndex();
        searchPath.clear();

        AZ_Warning("BoneMapHandler", false, "Finish auto mapping.");
    }

    void BoneMapWidget::UpdateAllBones(AZ::SceneAPI::Containers::SceneGraph& graph)
    {
        for (int i = 0; i < m_bones.size(); i++)
        {
            UpdateBone(graph, i);
        }
    }

    void BoneMapWidget::UpdateBone(AZ::SceneAPI::Containers::SceneGraph& graph, int index)
    {
        const SkeletonProfileBone& bone = m_bones[index];

        BoneTargetItem::BoneMapState state = BoneTargetItem::BONE_MAP_STATE_UNSET;

        // Figure out the state
        if (m_boneMapping.HasBone(bone.m_bone_name))
        {
            state = BoneTargetItem::BONE_MAP_STATE_SET;

            // If the profile bone specifies a parent, we need to make sure that it is correct
            if (!bone.m_bone_parent.empty())
            {
                // FIXME: Not sure if this is 100% correct but it works for the time being
                if (m_boneMapping.HasBone(bone.m_bone_parent))
                {
                    const AZStd::string& origBone = m_boneMapping.GetOrigBone(bone.m_bone_name);
                    const AZStd::string& origParentBone = m_boneMapping.GetOrigBone(bone.m_bone_parent);

                    NodeIndex boneIndex = graph.Find(origBone);
                    NodeIndex boneParentIndex = graph.Find(origParentBone);

                    if (graph.GetNodeParent(boneIndex) != boneParentIndex)
                    {
                        state = BoneTargetItem::BONE_MAP_STATE_ERROR;
                    }
                }
            }
        }
        else if (bone.m_require)
        {
            state = BoneTargetItem::BONE_MAP_STATE_MISSING;
        }

        bone.m_boneTarget->SetState(state);
    }

    AZ::SceneAPI::Containers::SceneGraph& BoneMapWidget::GetGraph()
    {
        AZ::SceneAPI::UI::ManifestWidget* mainWidget = AZ::SceneAPI::UI::ManifestWidget::FindRoot(this);
        AZ_Assert(mainWidget, "NodeListSelectionWidget is not an (in)direct child of the ManifestWidget.");
        return mainWidget->GetScene()->GetGraph();
    }

    QWidget* BoneMapHandler::CreateGUI(QWidget* parent)
    {
        BoneMapWidget* widget = aznew BoneMapWidget(parent);

        connect(widget, &BoneMapWidget::mappingChanged, this, [widget]()
        {
            AzToolsFramework::PropertyEditorGUIMessages::Bus::Broadcast(
                [widget](AzToolsFramework::PropertyEditorGUIMessages* handler)
                {
                    handler->RequestWrite(widget);
                    handler->OnEditingFinished(widget);
                    handler->RequestRefresh(AzToolsFramework::Refresh_EntireTree);
                });
        });

        return widget;
    }

    AZ::u32 BoneMapHandler::GetHandlerName() const
    {
        return AZ_CRC_CE("BoneMapHandler");
    }

    void BoneMapHandler::ConsumeAttribute(BoneMapWidget* widget, AZ::u32 attrib, AzToolsFramework::PropertyAttributeReader* attrValue, [[maybe_unused]] const char* debugName)
    {
        if (attrib == AZ::Edit::Attributes::ReadOnly)
        {
            bool readOnly;
            if (attrValue->Read<bool>(readOnly))
            {
                widget->setEnabled(!readOnly);
            }
        }
    }

    void BoneMapHandler::WriteGUIValuesIntoProperty([[maybe_unused]] size_t index, BoneMapWidget* widget, property_t& instance, [[maybe_unused]] AzToolsFramework::InstanceDataNode* node)
    {
        instance = widget->GetBoneMap();
    }

    bool BoneMapHandler::ReadValuesIntoGUI([[maybe_unused]] size_t index, BoneMapWidget* widget, const property_t& instance, [[maybe_unused]] AzToolsFramework::InstanceDataNode* node)
    {
        widget->SetBoneMap(instance);
        return true;
    }
}
