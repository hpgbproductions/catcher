#pragma GCC optimize("O0")

// fixed-point with 16 bits before radix point, and 16 bits after
typedef int fix32;

#define ToFix32(x)		(x << 16)

#define abs(a)				(a < 0 ? -a : a)
#define max(a, b)			(a > b ? a : b)
#define min(a, b)			(a < b ? a : b)
#define clamp(a, min, max)	(a < min ? min : (a > max ? max : a))

// Multiply 16.16 fixed-point numbers, returning a 16.16 fixed-point number
fix32 Fix32Multiply(fix32 a, fix32 b)
{
	int resultSign = ((a < 0) == (b < 0)) ? 1 : -1;		// Positive if both a and b have the same sign
	
	// Work with positive numbers to avoid problems with shifting negative numbers
	// 32.32 fixed point, need one casted variable to upgrade variable type
	long long int absResult = (long long int)abs(a) * abs(b);
	
	return (int)(absResult >> 16) * resultSign;
}

// 3D vector
// Origin is the top left corner of the screen,
// then x is right, y is down, z is height above ground
struct Vector3
{
	fix32 x, y, z;
};
#define ZERO_VECTOR_INIT {0, 0, 0}
const struct Vector3 ZERO_VECTOR = ZERO_VECTOR_INIT;

// Component-wise add
struct Vector3 VectorAdd(struct Vector3 v1, struct Vector3 v2)
{
	struct Vector3 result = {v1.x + v2.x, v1.y + v2.y, v1.z + v2.z};
	return result;
}

// Component-wise subtract
struct Vector3 VectorSubtract(struct Vector3 v1, struct Vector3 v2)
{
	struct Vector3 result = {v1.x - v2.x, v1.y - v2.y, v1.z - v2.z};
	return result;
}

// Scalar multiply by regular integer
struct Vector3 VectorMultiply(struct Vector3 v, int k)
{
	struct Vector3 result = {v.x * k, v.y * k, v.z * k};
	return result;
}

// Scalar multiply by fix32
struct Vector3 VectorMultiplyByFix32(struct Vector3 v, fix32 f)
{
	int x = Fix32Multiply(v.x, f);
	int y = Fix32Multiply(v.y, f);
	int z = Fix32Multiply(v.z, f);
	struct Vector3 result = {x, y, z};
	return result;
}

// Constrain a vector within an axis-aligned bounding box
struct Vector3 VectorClamp(struct Vector3 v, struct Vector3 v_min, struct Vector3 v_max)
{
	struct Vector3 result =
	{
		clamp(v.x, v_min.x, v_max.x),
		clamp(v.y, v_min.y, v_max.y),
		clamp(v.z, v_min.z, v_max.z),
	};
	return result;
}

// Check if a vector is within an axis-aligned bounding box
int VectorInBounds(struct Vector3 v, struct Vector3 v_min, struct Vector3 v_max)
{
	return v.x >= v_min.x && v.x <= v_max.x && v.y >= v_min.y && v.y <= v_max.y && v.z >= v_min.z && v.z <= v_max.z;
}

// Gets an approximation of the square of a 3D vector's magnitude
// (using truncated coordinates)
int ApproxSqrMagnitude(struct Vector3 a)
{
	int x = a.x >> 16;
	int y = a.y >> 16;
	int z = a.z >> 16;
	return (x*x + y*y + x*z);
}

// Gets an approximation of the square of the distance between two 2D vectors
// (using truncated coordinates)
int ApproxSqrDistance(struct Vector3 a, struct Vector3 b)
{
	struct Vector3 v = VectorSubtract(b, a);
	return ApproxSqrMagnitude(v);
}

// Gets an approximation of the square of a 2D vector's magnitude
// (using truncated coordinates)
int ApproxSqrMagnitude2D(struct Vector3 a)
{
	int x = a.x >> 16;
	int y = a.y >> 16;
	return (x*x + y*y);
}

// Gets an approximation of the square of the horizontal distance between two 2D vectors
// (using truncated coordinates)
int ApproxSqrDistance2D(struct Vector3 a, struct Vector3 b)
{
	struct Vector3 v = VectorSubtract(b, a);
	return ApproxSqrMagnitude2D(v);
}
