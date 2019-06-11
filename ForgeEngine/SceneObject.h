#pragma once
#include <DirectXMath.h>

class Transform;

class SceneObject
{
public:
    SceneObject(DirectX::XMFLOAT3 pos);
    virtual ~SceneObject();

    inline Transform* GetTransform() { return m_transform; }
    virtual void Update() {}

protected:
    Transform* m_transform;
};
