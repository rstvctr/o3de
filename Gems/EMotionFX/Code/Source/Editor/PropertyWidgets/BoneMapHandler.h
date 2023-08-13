#pragma once

#if !defined(Q_MOC_RUN)
#include <AzToolsFramework/UI/PropertyEditor/PropertyEditorAPI.h>
#include <EMotionFX/Pipeline/SceneAPIExt/Rules/SkeletonRemapRule.h>
#include <EMotionFX/Source/Transform.h>
#include <QBitmap>
#include <QGraphicsItem>
#include <QWidget>
#endif

class QGraphicsScene;
class QGraphicsSvgItem;
class QGraphicsView;
class QLineEdit;

namespace EMotionFX
{
    class ActorJointPicker;

    class BoneTargetItem : public QGraphicsItem
    {
    public:
        using SelectedCallback = AZStd::function<void(BoneTargetItem*)>;
        enum BoneMapState {
            BONE_MAP_STATE_UNSET,
            BONE_MAP_STATE_SET,
            BONE_MAP_STATE_MISSING,
            BONE_MAP_STATE_ERROR
        };

        BoneTargetItem(bool selected, BoneMapState state, AZStd::string nam);
        QRectF boundingRect() const override;

        void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

        void SetState(BoneMapState state);

        const AZStd::string& GetName() const { return m_name; }

    protected:
        AZStd::string m_name;
        BoneMapState m_state = BONE_MAP_STATE_ERROR;

        QPixmap m_selectedPixmap;
        QPixmap m_unselectedPixmap;
        QBitmap m_circleMask;
    };

    class BoneMapWidget : public QWidget
    {
        Q_OBJECT //AUTOMOC

    public:
    	enum TailDirection {
            TAIL_DIRECTION_AVERAGE_CHILDREN,
            TAIL_DIRECTION_SPECIFIC_CHILD,
            TAIL_DIRECTION_END
        };

        struct SkeletonProfileGroup {
            AZStd::string m_groupName;
            AZStd::string m_textureName;
            QGraphicsSvgItem* m_imageItem = nullptr;
            AZStd::vector<BoneTargetItem*> m_boneTargets;
        };

        struct SkeletonProfileBone {
            AZStd::string m_bone_name;
            AZStd::string m_bone_parent;
            TailDirection m_tail_direction = TAIL_DIRECTION_AVERAGE_CHILDREN;
            AZStd::string m_bone_tail;
            AZ::Vector2 m_handle_offset;
            AZStd::string m_group;
            bool m_require = false;
        };

        AZ_CLASS_ALLOCATOR_DECL
        BoneMapWidget(QWidget* parent);

    signals:

    private slots:
        void onGraphicsSceneSelectionChanged();
        void onGuessBoneMappingsButtonClicked();
        void onClearButtonClicked();
        void onGroupSelectCurrentIndexChanged(int index);
        void onBonePickerSelectionChanged();

    private:
        void InitBonesAndGroups();

        QGraphicsScene* m_graphicsScene = nullptr;
        QGraphicsView* m_graphicsView = nullptr;
        QLabel* m_targetNameLabel = nullptr;
        QLineEdit* m_boneNameLineEdit = nullptr;
        ActorJointPicker* m_bonePicker = nullptr;

        BoneTargetItem* m_currentSelectedTarget = nullptr;

        AZStd::vector<SkeletonProfileGroup> m_groups;
        AZStd::vector<SkeletonProfileBone> m_bones;
    };

    class BoneMapHandler
        : public QObject
        , public AzToolsFramework::PropertyHandler<BoneMap, BoneMapWidget>
    {
        Q_OBJECT

    public:
        AZ_CLASS_ALLOCATOR_DECL

        QWidget* CreateGUI(QWidget* parent) override;
        bool AutoDelete() const override { return false; }

        AZ::u32 GetHandlerName() const override;

        void ConsumeAttribute(BoneMapWidget* widget, AZ::u32 attrib, AzToolsFramework::PropertyAttributeReader* attrValue, const char* debugName) override;

        void WriteGUIValuesIntoProperty(size_t index, BoneMapWidget* GUI, property_t& instance, AzToolsFramework::InstanceDataNode* node) override;

        bool ReadValuesIntoGUI(size_t index, BoneMapWidget* GUI, const property_t& instance, AzToolsFramework::InstanceDataNode* node) override;
    };
}
