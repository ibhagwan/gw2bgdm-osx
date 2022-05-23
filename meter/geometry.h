#include <wtypes.h>
#include <stdint.h>

typedef struct _Vec2i {
	int x;
	int y;
} Vec2i;

typedef struct _Vec2f {
	float x;
	float y;
} Vec2f;

typedef struct _Vec3f {
	float x;
	float y;
	float z;
} Vec3f;

typedef struct __Matrix44 {
	union {
		struct {
			float        _11, _12, _13, _14;
			float        _21, _22, _23, _24;
			float        _31, _32, _33, _34;
			float        _41, _42, _43, _44;

		};
		float x[4][4];
	};
} Matrix44;



__inline Matrix44* Matrix44New (Matrix44 *pOut);
__inline BOOL Matrix44IsNew (const Matrix44 *pM);

Matrix44* Matrix44Multiply(Matrix44 *pOut, const Matrix44 *pM1, const Matrix44 *pM2);
Matrix44* Matrix44Transpose(Matrix44 *pOut, CONST Matrix44 *pM);
Matrix44* Matrix44Inverse(Matrix44 *pOut, CONST Matrix44 *pM);
Vec3f* Matrix44MultVec(Vec3f *pOut, CONST Matrix44 *pM, CONST Vec3f* pV);
Vec3f* Matrix44MultDir(Vec3f *pOut, CONST Matrix44 *pM, CONST Vec3f* pV);

BOOL computePixelCoordinates(
	Vec2i *pRaster,
	const Vec3f *pWorld,
	const Matrix44 *worldToCamera,
	const float canvasWidth,
	const float canvasHeight,
	const uint32_t imageWidth,
	const uint32_t imageHeight
);