#pragma once

#include "MathLib.h"

class FLight
{
public:
	FLight();

	const Vector3f& GetColor() const { return m_Color; }
	void SetColor(const Vector3f& Color) { m_Color = Color; }

protected:
	Vector3f m_Position;
	Vector3f m_Color;
};


class FDirectionalLight : public FLight
{
public:
	FDirectionalLight();
	
	void SetDirection(const Vector3f& Direction);
	const Vector3f& GetDirection() const { return m_Direction; }
	
	const FMatrix& GetViewMatrix() const;
	const FMatrix& GetProjectMatrix();
	const FMatrix& GetLightToShadowMatrix();

	void SetIntensity(float Intensity);
	float GetIntensity() const { return m_Intensity; }
	
	void UpdateShadowBound(const FBoundingBox& WorldBound);

private:
	void UpdateShadowMatrix();


protected:
	Vector3f m_Direction;
	float m_Intensity;
	FMatrix m_ViewMat, m_ProjMat, m_ShadowMatrix;
};