﻿#pragma once

#include <math.h>
#include <stdint.h>
#include <intrin.h>
#include <functional>

const float MATH_PI = 3.141592654f;

template<typename T>
struct Vector4;

template<typename T>
struct Vector2
{
	union 
	{
		T data[2];
		struct { T x, y;};
	};
	
	T& operator [] (int i)
	{
		return data[i];
	}
	Vector2() : x(0), y(0) {}
	Vector2(T c) : x(c), y(c) {}
	Vector2(T x, T y) : x(x), y(y) {}
};

template<typename T>
struct Vector3
{
	union 
	{
		T data[3];
		struct { T x, y, z;};
	};
	T& operator [] (int i)
	{
		return data[i];
	}
	const T& operator [] (int i) const
	{
		return data[i];
	}
	Vector3() : x(0), y(0), z(0) {}
	Vector3(T c) : x(c), y(c), z(c) {}
	Vector3(T x, T y, T z) : x(x), y(y), z(z) {}
	Vector3(const Vector4<T>& v) : x(v.x), y(v.y), z(v.z) {}

	T Dot(const Vector3& rhs) const
	{
		return x * rhs.x + y * rhs.y + z * rhs.z;
	}

	Vector3& operator += (const Vector3& rhs)
	{
		x += rhs.x;
		y += rhs.y;
		z += rhs.z;
		return *this;
	}

	Vector3 operator + (const Vector3& rhs) const
	{
		Vector3 Res(*this);
		Res += rhs;
		return Res;
	}

	Vector3 operator * (const Vector3& rhs) const
	{
		return Vector3(x * rhs.x, y * rhs.y, z * rhs.z);
	}

	Vector3 operator - (const Vector3& rhs) const
	{
		return Vector3(x - rhs.x, y - rhs.y, z - rhs.z);
	}

	T Length() const
	{
		return (T)sqrt(x * x + y * y + z * z);
	}

	Vector3 Normalize() const
	{
		T length = Length();
		T inv = T(1) / length;
		return Vector3(x * inv, y * inv, z *inv);
	}
};

template<typename T, typename scalar>
Vector3<T> operator * (const Vector3<T>& lhs, scalar s)
{
	return Vector3<T>(lhs.x * s, lhs.y * s, lhs.z * s); 
}

template<typename T, typename scalar>
Vector3<T> operator * (scalar s, const Vector3<T>& lhs)
{
	return Vector3<T>(lhs.x * s, lhs.y * s, lhs.z * s); 
}

template<typename T>
Vector3<T> Cross(const Vector3<T>& lhs, const Vector3<T>& rhs)
{
	return Vector3<T>(lhs.y * rhs.z - lhs.z * rhs.y, lhs.z * rhs.x - lhs.x * rhs.z, lhs.x * rhs.y - lhs.y * rhs.x);
}

template<typename T>
struct Vector4
{
	union 
	{
		T data[4];
		struct { T x, y, z, w;};
	};
	T& operator [] (int i)
	{
		return data[i];
	}
	const T& operator [] (int i) const
	{
		return data[i];
	}
	Vector4() : x(0), y(0), z(0), w(0) {}
	Vector4(T c) : x(c), y(c), z(c), w(c) {}
	Vector4(T x, T y, T z, T w) : x(x), y(y), z(z), w(w) {}
	Vector4(const Vector3<T>& v, T s = 0) : x(v.x), y(v.y), z(v.z), w(s) {}
	T Dot(const Vector4& rhs) const
	{
		return x * rhs.x + y * rhs.y + z * rhs.z + w * rhs.w;
	}
};



typedef Vector2<int> Vector2i;
typedef Vector2<float> Vector2f;
typedef Vector3<float> Vector3f;
typedef Vector4<float> Vector4f;



struct FMatrix
{
	union 
	{
		Vector4f row[4];
		struct { Vector4f r0, r1, r2, r3;};
	};
	FMatrix();

	FMatrix(const Vector3f& r0, const Vector3f& r1, const Vector3f& r2, const Vector3f& r3);

	FMatrix(const Vector4f& r0, const Vector4f& r1, const Vector4f& r2, const Vector4f& r3);
	FMatrix(float m00, float m01, float m02, float m03,
			float m10, float m11, float m12, float m13,
			float m20, float m21, float m22, float m23,
			float m30, float m31, float m32, float m33);

	Vector4f Column(int i) const;

	FMatrix operator * (const FMatrix& rhs);
	FMatrix& operator *= (const FMatrix& rhs);

	Vector3f TranslateVector(const Vector3f& vector);
	Vector3f TransformPosition(const Vector3f& position);
	FMatrix Transpose();

	static FMatrix TranslateMatrix(const Vector3f& T);
	static FMatrix ScaleMatrix(float s);
	static FMatrix RotateX(float v);
	static FMatrix RotateY(float v);
	static FMatrix RotateZ(float v);
	static FMatrix MatrixRotationRollPitchYaw(float Roll, float Pitch, float Yaw);
	static FMatrix MatrixLookAtLH(const Vector3f& EyePosition, const Vector3f& FocusPosition, const Vector3f& UpDirection);
	static FMatrix MatrixPerspectiveFovLH(float FovAngleY, float AspectHByW, float NearZ, float FarZ);
	static FMatrix MatrixOrthoLH(float Width, float Height, float NearZ, float FarZ);
};

//inline Vector4f operator*(const FMatrix& mat, const Vector4f& vec);

inline Vector4f operator*(const Vector4f& vec, const FMatrix& mat);

template <typename T>
T AlignUpWithMask(T Value, size_t Mask)
{
	return (T)(((size_t)Value + Mask) & (~Mask));
}

template <typename T>
T AlignUp(T Value, size_t Alignment)
{
	return AlignUpWithMask(Value, Alignment - 1);
}

template <typename T>
T AlignDownWithMask(T value, size_t mask)
{
	return (T)((size_t)value & ~mask);
}

template <typename T>
T AlignDown(T value, size_t alignment)
{
	return AlignDownWithMask(value, alignment - 1);
}


#ifdef _M_X64
#define ENABLE_SSE_CRC32 1
#else
#define ENABLE_SSE_CRC32 0
#endif

#if ENABLE_SSE_CRC32
#pragma intrinsic(_mm_crc32_u32)
#pragma intrinsic(_mm_crc32_u64)
#endif

inline size_t HashRange(const uint32_t* const Begin, const uint32_t* const End, size_t Hash)
{
#if ENABLE_SSE_CRC32
	const uint64_t* Iter64 = (const uint64_t*)AlignUp(Begin, 8);
	const uint64_t* const End64 = (const uint64_t* const)AlignDown(End, 8);

	// If not 64-bit aligned, start with a single u32
	if ((uint32_t*)Iter64 > Begin)
		Hash = _mm_crc32_u32((uint32_t)Hash, *Begin);

	// Iterate over consecutive u64 values
	while (Iter64 < End64)
		Hash = _mm_crc32_u64((uint64_t)Hash, *Iter64++);

	// If there is a 32-bit remainder, accumulate that
	if ((uint32_t*)Iter64 < End)
		Hash = _mm_crc32_u32((uint32_t)Hash, *(uint32_t*)Iter64);
#else
	// An inexpensive hash for CPUs lacking SSE4.2
	for (const uint32_t* Iter = Begin; Iter < End; ++Iter)
		Hash = 16777619U * Hash ^ *Iter;
#endif

	return Hash;
}

template <typename T> inline size_t HashState(const T* StateDesc, size_t Count = 1, size_t Hash = 2166136261U)
{
	static_assert((sizeof(T) & 3) == 0 && alignof(T) >= 4, "State object is not word-aligned");
	return HashRange((uint32_t*)StateDesc, (uint32_t*)(StateDesc + Count), Hash);
}