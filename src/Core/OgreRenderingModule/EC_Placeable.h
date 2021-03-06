// For conditions of distribution and use, see copyright notice in license.txt

#pragma once

#include "IComponent.h"
#include "IAttribute.h"
#include "EntityReference.h"
#include "OgreModuleApi.h"
#include "OgreModuleFwd.h"
#include "Transform.h"
#include "Math/float3.h"
#include "Math/MathFwd.h"

namespace Ogre { class Bone; }

/// Ogre placeable (scene node) component
/**
<table class="header">
<tr>
<td>
<h2>Placeable</h2>
Ogre (scene node) component.

Registered by OgreRenderer::OgreRenderingModule.

<b>Attributes</b>:
<ul>
<li>Transform: transform
<div>Sets the position, rotation and scale of the entity.</div>
<li>bool: drawDebug
<div>Shows the debug bounding box of geometry attached to the placeable.</div>
<li>int: selectionLayer
<div>Selection layer for raycasts.</div>
<li>EntityReference: parentRef
<div>The entity to attach to. The entity in question needs to have EC_Placeable as well to work correctly</div>
<li>string: parentBone
<div>The bone to attach to. The parent entity needs to have a skeletal EC_Mesh component</div>
</ul>

<b>Exposes the following scriptable functions:</b>
<ul>
<li>"Show": Shows the Entity
<li>"Hide": Hides the Entity
<li>"ToggleVisibility": Toggles visibility
</ul>

<b>Reacts on the following actions:</b>
<ul>
<li>"ShowEntity": Shows the Entity
<li>"HideEntity": Hides the Entity
<li>"ToggleEntity": Toggles visibility
</ul>
</td>
</tr>

Does not emit any actions.

<b>Doesn't depend on any components</b>.

</table>
*/
class OGRE_MODULE_API EC_Placeable : public IComponent
{
    Q_OBJECT
    COMPONENT_NAME("EC_Placeable", 20)

public:
    /// Do not directly allocate new components using operator new, but use the factory-based SceneAPI::CreateComponent functions instead.
    explicit EC_Placeable(Scene* scene);
    virtual ~EC_Placeable();

    /// Stores the position, rotation and scale of this sceene node in the coordinate space of its parent.
    Q_PROPERTY(Transform transform READ gettransform WRITE settransform);
    DEFINE_QPROPERTY_ATTRIBUTE(Transform, transform);

    /// If true, the bounding box of the mesh in this entity is shown (for debugging purposes).
    Q_PROPERTY(bool drawDebug READ getdrawDebug WRITE setdrawDebug);
    DEFINE_QPROPERTY_ATTRIBUTE(bool, drawDebug);

    /// Specifies whether any objects attached to the scene node of this placeable are visible or not.
    Q_PROPERTY(bool visible READ getvisible WRITE setvisible);
    DEFINE_QPROPERTY_ATTRIBUTE(bool, visible);

    /// Specifies the selection layer of this object. This can be used to selectively perform raycasts.
    Q_PROPERTY(int selectionLayer READ getselectionLayer WRITE setselectionLayer)
    DEFINE_QPROPERTY_ATTRIBUTE(int, selectionLayer);

    /// Specifies the parent entity of this entity. Set to 0 for no parenting.
    Q_PROPERTY(EntityReference parentRef READ getparentRef WRITE setparentRef)
    DEFINE_QPROPERTY_ATTRIBUTE(EntityReference, parentRef);

    /// Specifies the name of the bone on the parent entity.
    /// Needs that the parent entity has a skeletal mesh. 
    /// Set to empty for no parent bone assignment, in which case this scene node is attached to the root of the parent node.
    Q_PROPERTY(QString parentBone READ getparentBone WRITE setparentBone)
    DEFINE_QPROPERTY_ATTRIBUTE(QString, parentBone);

    /// Returns the Ogre scene node for attaching geometry.
    /** Do not manipulate the pos/orientation/scale of this node directly, but instead use the Transform property. */
    Ogre::SceneNode* GetSceneNode() const { return sceneNode_; }

public slots:
    /// Sets the translation part of this placeable's transform.
    /// @note This function sets the Transform attribute of this component, and synchronizes to network.
    void SetPosition(float x, float y, float z);
    void SetPosition(const float3 &pos);

    /// Sets the orientation of this placeable's transform.
    /// If you want to set the orientation of this placeable using Euler angles, use e.g. 
    /// the Quat::FromEulerZYX function.
    /// @note This function sets the Transform attribute of this component, and synchronizes to network.
    /// @note This function preserves the previous position and scale of this transform.
    void SetOrientation(const Quat &q);

    /// Sets the rotation and scale of this placeable (the local-to-parent transform).
    /// @param rotAndScale The transformation matrix to set. This matrix is assumed to be orthogonal (no shear), 
    ///                    and can not contain any mirroring.
    /// @note This function sets the Transform attribute of this component, and synchronizes to network.
    /// @note This function preserves the previous position of this transform.
    void SetOrientationAndScale(const float3x3 &rotAndScale);
    void SetOrientationAndScale(const Quat &q, const float3 &scale);

    /// Sets the scale of this placeable's transform.
    /// @note Due to very odd Ogre3D behavior, hierarchical nodes are concatenated in a very unstandard manner.
    ///       The Ogre way for concatenating two Transforms T1 * R1 * S1 and T2 * R2 * S2 results in the transform
    ///       T1 * R1 * (S1(T2)) * R2 * S1 * R2, where S1(T2) is the scale applied to the translation T2.
    ///       The normal way would be to produce a concatenation T1 * R1 * S1 * T2 * R2 * S2.
    ///       Therefore, expect to receive odd results if using scales in a hierarchy.
    /// @note This function preserves the previous translation and rotation of this placeable.
    void SetScale(float x, float y, float z);
    void SetScale(const float3 &scale);

    /// Sets the position, rotation and scale of this placeable (the local-to-parent transform).
    /// @param tm An orthogonal matrix (no shear), which cannot contain mirroring. The float4x4 version is provided
    ///           for conveniency, and the last row must be identity [0 0 0 1].
    /// @note This function sets the Transform attribute of this component, and synchronizes to network.
    /// @note Logically, the matrix tm is applied to the object first before translating by pos.
    void SetTransform(const float3x3 &tm, const float3 &pos);
    void SetTransform(const float3x4 &tm);
    void SetTransform(const float4x4 &tm);

    /// Sets the position and rotation of this placeable (the local-to-parent transform).
    /// @note This function RESETS the scale of this transform to (1,1,1).
    /// @note Logically, the matrix tm is applied to the object first before translating by pos.
    void SetTransform(const Quat &orientation, const float3 &pos);
    /// Sets the position, rotation and scale of this placeable (the local-to-parent transform).
    /// @note Logically, the order of transformations is T * R * S * v.
    void SetTransform(const Quat &orientation, const float3 &pos, const float3 &scale);

    /// Sets the transform of this placeable by specifying the world-space transform this scene node should have.
    /// This function recomputes the local->parent transform for this placeable so that the resulting world transform is as given.
    /// @param tm An orthogonal matrix (no shear), which cannot contain mirroring. The float4x4 version is provided
    ///           for conveniency, and the last row must be identity [0 0 0 1].
    /// @note This function sets the Transform attribute of this component, and synchronizes to network.
    /// @note Logically, the matrix tm is applied to the object first before translating by pos.
    void SetWorldTransform(const float3x3 &tm, const float3 &pos);
    void SetWorldTransform(const float3x4 &tm);
    void SetWorldTransform(const float4x4 &tm);

    /// Sets the transform of this placeable by specifying the world-space transform this scene node should have.
    /// @note This function RESETS the scale of this transform to (1,1,1).
    /// @note Logically, the matrix tm is applied to the object first before translating by pos.
    void SetWorldTransform(const Quat &orientation, const float3 &pos);
    /// Sets the transform of this placeable by specifying the world-space transform this scene node should have.
    /// @note Logically, the order of transformations is T * R * S * v.
    void SetWorldTransform(const Quat &orientation, const float3 &pos, const float3 &scale);

    /// Returns the position of this placable node in world space.
    float3 WorldPosition() const;

    /// Returns the orientation of this placeable node in world space.
    Quat WorldOrientation() const;

    /// Returns the scale of this placeable node in world space.
    float3 WorldScale() const;

    /// Returns the position of this placeable node in the space of its parent.
    float3 Position() const;

    /// Returns the orientation of this placeable node in the space of its parent.
    Quat Orientation() const;

    /// Returns the scale of this placeable node in the space of its parent.
    float3 Scale() const;

    /// Returns the concatenated world transformation of this placeable.
    float3x4 LocalToWorld() const;
    /// Returns the matrix that transforms objects from world space into the local coordinate space of this placeable.
    float3x4 WorldToLocal() const;

    /// Returns the local transformation of this placeable in the space of its parent.
    /// @note For a placeable which is not attached to any parent, this returns the same transform as LocalToWorld().
    float3x4 LocalToParent() const;
    /// Returns the matrix that transforms objects from this placeable's parent's space into the local coordinate 
    /// space of this placeable.
    /// @note For a placeable which is not attached to any parent, this returns the same transform as WorldToLocal().
    float3x4 ParentToLocal() const;

    /// Shows the entity
    /** @note Doesn't alter the component's visible attribute. */
    void Show();

    /// Hides the entity.
    /** @note Doesn't alter the component's visible attribute. */
    void Hide();

    /// Toggles visibility.
    /** @note Doesn't alter the component's visible attribute. */
    void ToggleVisibility();

    /// Return whether is attached to the Ogre scene node hierarchy
    bool IsAttached() const { return attached_; }

    /// Re-parents this scene node to the given parent scene node. The parent entity must contain an EC_Placeable component.
    /// Detaches this placeable from its previous parent.
    /// @param preserveWorldTransform If true, the world space position of this placeable is preserved.
    ///                               If false, the transform attibute of this placeable is treated as the new local->parent transform for this placeable.
    /// @note This function sets the parentRef and parentBone attributes of this component to achieve the parenting.
    void SetParent(Entity *parent, bool preserveWorldTransform);

    /// Re-parents this scene node to the named bone of the given parent scene node. The parent scene node must contain an EC_Placeable component and an EC_Mesh with a skeleton.
    /// Detaches this placeable from its previous parent.
    /// @param preserveWorldTransform If true, the world space position of this placeable is preserved.
    ///                               If false, the transform attibute of this placeable is treated as the new local->parent transform for this placeable.
    /// @note This function sets the parentRef and parentBone attributes of this component to achieve the parenting.
    void SetParent(Entity *parent, QString boneName, bool preserveWorldTransform);

    /// Returns all entities that are attached to this placeable.
    EntityList Children();

    /// Prints the scene node hierarchy this scene node is part of.
    void DumpNodeHierarhy();
signals:
    /// Emitted when about to be destroyed
    void AboutToBeDestroyed();

private slots:
    /// Handle attributechange
    /** @param attribute Attribute that changed.
        @param change Change type. */
    void HandleAttributeChanged(IAttribute* attribute, AttributeChange::Type change);

    /// Registers the action this EC provides to the parent entity, when it's set.
    void RegisterActions();
    
    /// Handle destruction of the parent mesh
    void OnParentMeshDestroyed();
            
    /// Handle destruction of the parent placeable
    void OnParentPlaceableDestroyed();
        
    /// Handle late creation of the parent entity, and try attaching to it
    void CheckParentEntityCreated(Entity* entity, AttributeChange::Type change);
    
    /// Handle change of the parent mesh
    void OnParentMeshChanged();
    
    /// Handle a component being added to the parent entity, in case it is the missing component we need
    void OnComponentAdded(IComponent* component, AttributeChange::Type change);

private:
    /// attaches scenenode to parent
    void AttachNode();
    
    /// detaches scenenode from parent
    void DetachNode();
    
    /// Ogre world ptr
    OgreWorldWeakPtr world_;
    
    /// Ogre scene node for geometry. This always exists as long as the EC_Placeable is alive
    Ogre::SceneNode* sceneNode_;
    
    /// Ogre scene node for manual bone attachment
    Ogre::SceneNode* boneAttachmentNode_;

    /// The bone we are tracking in bone attachment mode
    Ogre::Bone* parentBone_;
    
    /// Parent placeable, if any
    EC_Placeable* parentPlaceable_;
    
    /// Parent mesh in bone attachment mode
    EC_Mesh* parentMesh_;

    /// attached to scene hierarchy-flag
    bool attached_;

    friend class BoneAttachmentListener;
    friend class CustomTagPoint;
};
