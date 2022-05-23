#include <float.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include "geometry.h"



//--------------------------
// 4D Matrix
//--------------------------

__inline Matrix44* Matrix44New
(Matrix44 *pOut)
{
	if (!pOut)
		return NULL;

	pOut->x[0][1] = pOut->x[0][2] = pOut->x[0][3] =
		pOut->x[1][0] = pOut->x[1][2] = pOut->x[1][3] =
		pOut->x[2][0] = pOut->x[2][1] = pOut->x[2][3] =
		pOut->x[3][0] = pOut->x[3][1] = pOut->x[3][2] = 0.0f;

	pOut->x[0][0] = pOut->x[1][1] = pOut->x[2][2] = pOut->x[3][3] = 1.0f;
	return pOut;
}


__inline BOOL Matrix44IsNew
(const Matrix44 *pM)
{
	if (!pM)
		return FALSE;

	return pM->x[0][0] == 1.0f && pM->x[0][1] == 0.0f && pM->x[0][2] == 0.0f && pM->x[0][3] == 0.0f &&
		pM->x[1][0] == 0.0f && pM->x[1][1] == 1.0f && pM->x[1][2] == 0.0f && pM->x[1][3] == 0.0f &&
		pM->x[2][0] == 0.0f && pM->x[2][1] == 0.0f && pM->x[2][2] == 1.0f && pM->x[2][3] == 0.0f &&
		pM->x[3][0] == 0.0f && pM->x[3][1] == 0.0f && pM->x[3][2] == 0.0f && pM->x[3][3] == 1.0f;
}


//[comment]
// To make it easier to understand how a matrix multiplication works, the fragment of code
// included within the #if-#else statement, show how this works if you were to iterate
// over the coefficients of the resulting matrix (a). However you will often see this
// multiplication being done using the code contained within the #else-#end statement.
// It is exactly the same as the first fragment only we have litteraly written down
// as a series of operations what would actually result from executing the two for() loops
// contained in the first fragment. It is supposed to be faster, however considering
// matrix multiplicatin is not necessarily that common, this is probably not super
// useful nor really necessary (but nice to have -- and it gives you an example of how
// it can be done, as this how you will this operation implemented in most libraries).
//[/comment]
Matrix44* Matrix44Multiply(Matrix44 *pOut, const Matrix44 *pM1, const Matrix44 *pM2)
{
#if 0
	for (uint8_t i = 0; i < 4; ++i) {
		for (uint8_t j = 0; j < 4; ++j) {
			c[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j] +
				a[i][2] * b[2][j] + a[i][3] * b[3][j];
		}
	}
#else
	// A restric qualified pointer (or reference) is basically a promise
	// to the compiler that for the scope of the pointer, the target of the
	// pointer will only be accessed through that pointer (and pointers
	// copied from it.
	const float * __restrict ap = &pM1->x[0][0];
	const float * __restrict bp = &pM2->x[0][0];
	float * __restrict cp = &pOut->x[0][0];

	float a0, a1, a2, a3;

	a0 = ap[0];
	a1 = ap[1];
	a2 = ap[2];
	a3 = ap[3];

	cp[0] = a0 * bp[0] + a1 * bp[4] + a2 * bp[8] + a3 * bp[12];
	cp[1] = a0 * bp[1] + a1 * bp[5] + a2 * bp[9] + a3 * bp[13];
	cp[2] = a0 * bp[2] + a1 * bp[6] + a2 * bp[10] + a3 * bp[14];
	cp[3] = a0 * bp[3] + a1 * bp[7] + a2 * bp[11] + a3 * bp[15];

	a0 = ap[4];
	a1 = ap[5];
	a2 = ap[6];
	a3 = ap[7];

	cp[4] = a0 * bp[0] + a1 * bp[4] + a2 * bp[8] + a3 * bp[12];
	cp[5] = a0 * bp[1] + a1 * bp[5] + a2 * bp[9] + a3 * bp[13];
	cp[6] = a0 * bp[2] + a1 * bp[6] + a2 * bp[10] + a3 * bp[14];
	cp[7] = a0 * bp[3] + a1 * bp[7] + a2 * bp[11] + a3 * bp[15];

	a0 = ap[8];
	a1 = ap[9];
	a2 = ap[10];
	a3 = ap[11];

	cp[8] = a0 * bp[0] + a1 * bp[4] + a2 * bp[8] + a3 * bp[12];
	cp[9] = a0 * bp[1] + a1 * bp[5] + a2 * bp[9] + a3 * bp[13];
	cp[10] = a0 * bp[2] + a1 * bp[6] + a2 * bp[10] + a3 * bp[14];
	cp[11] = a0 * bp[3] + a1 * bp[7] + a2 * bp[11] + a3 * bp[15];

	a0 = ap[12];
	a1 = ap[13];
	a2 = ap[14];
	a3 = ap[15];

	cp[12] = a0 * bp[0] + a1 * bp[4] + a2 * bp[8] + a3 * bp[12];
	cp[13] = a0 * bp[1] + a1 * bp[5] + a2 * bp[9] + a3 * bp[13];
	cp[14] = a0 * bp[2] + a1 * bp[6] + a2 * bp[10] + a3 * bp[14];
	cp[15] = a0 * bp[3] + a1 * bp[7] + a2 * bp[11] + a3 * bp[15];
#endif
	return pOut;
}

// \brief return a transposed copy of the current matrix as a new matrix
Matrix44* Matrix44Transpose(Matrix44 *pOut, CONST Matrix44 *pM)
{
#if 0
	Matrix44 t;
	for (uint8_t i = 0; i < 4; ++i) {
		for (uint8_t j = 0; j < 4; ++j) {
			t[i][j] = x[j][i];
		}
	}

	return t;
#else
	const float ** __restrict x = (const float **)&pM->x[0][0];
	pOut->x[0][0] = x[0][0];
	pOut->x[0][1] = x[1][0];
	pOut->x[0][2] = x[2][0];
	pOut->x[0][3] = x[3][0];
	pOut->x[1][0] = x[0][1];
	pOut->x[1][1] = x[1][1];
	pOut->x[1][2] = x[2][1];
	pOut->x[1][3] = x[3][1];
	pOut->x[2][0] = x[0][2];
	pOut->x[2][1] = x[1][2];
	pOut->x[2][2] = x[2][2];
	pOut->x[2][3] = x[3][2];
	pOut->x[3][0] = x[0][3];
	pOut->x[3][1] = x[1][3];
	pOut->x[3][2] = x[2][3];
	pOut->x[3][3] = x[3][3];
	return pOut;
#endif
}


//[comment]
// Compute the inverse of the matrix using the Gauss-Jordan (or reduced row) elimination method.
// We didn't explain in the lesson on Geometry how the inverse of matrix can be found. Don't
// worry at this point if you don't understand how this works. But we will need to be able to
// compute the inverse of matrices in the first lessons of the "Foundation of 3D Rendering" section,
// which is why we've added this code. For now, you can just use it and rely on it
// for doing what it's supposed to do. If you want to learn how this works though, check the lesson
// on called Matrix Inverse in the "Mathematics and Physics of Computer Graphics" section.
//[/comment]
Matrix44* Matrix44Inverse(Matrix44 *pOut, CONST Matrix44 *pM)
{
	int i, j, k;
	Matrix44 m = *pM;
	float ** __restrict s = (float **)&pOut->x[0][0];
	float ** __restrict t = (float **)&m.x[0][0];

	// Forward elimination
	for (i = 0; i < 3; i++) {
		int pivot = i;

		float pivotsize = t[i][i];

		if (pivotsize < 0)
			pivotsize = -pivotsize;

		for (j = i + 1; j < 4; j++) {
			float tmp = t[j][i];

			if (tmp < 0)
				tmp = -tmp;

			if (tmp > pivotsize) {
				pivot = j;
				pivotsize = tmp;
			}
		}

		if (pivotsize == 0) {
			// Cannot invert singular matrix
			return NULL;
		}

		if (pivot != i) {
			for (j = 0; j < 4; j++) {
				float tmp;

				tmp = t[i][j];
				t[i][j] = t[pivot][j];
				t[pivot][j] = tmp;

				tmp = s[i][j];
				s[i][j] = s[pivot][j];
				s[pivot][j] = tmp;
			}
		}

		for (j = i + 1; j < 4; j++) {
			float f = t[j][i] / t[i][i];

			for (k = 0; k < 4; k++) {
				t[j][k] -= f * t[i][k];
				s[j][k] -= f * s[i][k];
			}
		}
	}

	// Backward substitution
	for (i = 3; i >= 0; --i) {
		float f;

		if ((f = t[i][i]) == 0) {
			// Cannot invert singular matrix
			return NULL;
		}

		for (j = 0; j < 4; j++) {
			t[i][j] /= f;
			s[i][j] /= f;
		}

		for (j = 0; j < i; j++) {
			f = t[j][i];

			for (k = 0; k < 4; k++) {
				t[j][k] -= f * t[i][k];
				s[j][k] -= f * s[i][k];
			}
		}
	}

	return pOut;
}


//[comment]
// This method needs to be used for point-matrix multiplication. Keep in mind
// we don't make the distinction between points and vectors at least from
// a programming point of view, as both (as well as normals) are declared as Vec3.
// However, mathematically they need to be treated differently. Points can be translated
// when translation for vectors is meaningless. Furthermore, points are implicitly
// be considered as having homogeneous coordinates. Thus the w coordinates needs
// to be computed and to convert the coordinates from homogeneous back to Cartesian
// coordinates, we need to divided x, y z by w.
//
// The coordinate w is more often than not equals to 1, but it can be different than
// 1 especially when the matrix is projective matrix (perspective projection matrix).
//[/comment]
Vec3f* Matrix44MultVec(Vec3f *pOut, CONST Matrix44 *pM, CONST Vec3f* pV)
{
	const float ** __restrict x = (const float **)&pM->x[0][0];
	const float * __restrict src = &pV->x;
	float a, b, c, w;

	a = src[0] * x[0][0] + src[1] * x[1][0] + src[2] * x[2][0] + x[3][0];
	b = src[0] * x[0][1] + src[1] * x[1][1] + src[2] * x[2][1] + x[3][1];
	c = src[0] * x[0][2] + src[1] * x[1][2] + src[2] * x[2][2] + x[3][2];
	w = src[0] * x[0][3] + src[1] * x[1][3] + src[2] * x[2][3] + x[3][3];

	if (w)
	{
		pOut->x = a / w;
		pOut->y = b / w;
		pOut->z = c / w;
	}

	return pOut;
}


//[comment]
// This method needs to be used for vector-matrix multiplication. Look at the differences
// with the previous method (to compute a point-matrix multiplication). We don't use
// the coefficients in the matrix that account for translation (x[3][0], x[3][1], x[3][2])
// and we don't compute w.
//[/comment]

Vec3f* Matrix44MultDir(Vec3f *pOut, CONST Matrix44 *pM, CONST Vec3f* pV)
{
	const float ** __restrict x = (const float **)&pM->x[0][0];
	const float * __restrict src = &pV->x;
	float a, b, c;

	a = src[0] * x[0][0] + src[1] * x[1][0] + src[2] * x[2][0];
	b = src[0] * x[0][1] + src[1] * x[1][1] + src[2] * x[2][1];
	c = src[0] * x[0][2] + src[1] * x[1][2] + src[2] * x[2][2];

	pOut->x = a;
	pOut->y = b;
	pOut->z = c;

	return pOut;
}



//[comment]
// Compute the 2D pixel coordinates of a point defined in world space. This function
// requires the point original world coordinates of course, the world-to-camera
// matrix (which you can get from computing the inverse of the camera-to-world matrix,
// the matrix transforming the camera), the canvas dimension and the image width and
// height in pixels.
//
// Note that we don't check in this version of the function if the point is visible
// or not. If the absolute value of of any of the screen coordinates are greater
// that their respective maximum value (the canvas width for the x-coordinate,
// and the canvas height for the y-coordinate) then the point is not visible.
// When a SVG file is displayed to the screen, the application displaying the content
// of the file clips points and lines which are not visible. Thus we can store lines or point
// in the file whether they are visible or not. The final clipping will be done when the
// image is displayed to the screen.
//[/comment]
BOOL computePixelCoordinates(
	Vec2i *pRaster,
	const Vec3f *pWorld,
	const Matrix44 *worldToCamera,
	const float canvasWidth,
	const float canvasHeight,
	const uint32_t imageWidth,
	const uint32_t imageHeight
)
{
	if (canvasWidth == 0 || canvasHeight == 0)
		return FALSE;

	Vec2f pScreen = { 0.f, 0.f };
	Vec2f pNDC = { 0.f, 0.f };
	Vec3f pCamera = { 0.f, 0.f, 0.f };

	//Matrix44MultVec(&pCamera, worldToCamera, pWorld);

	if (pCamera.z == 0)
		return FALSE;

	pScreen.x = pCamera.x / -pCamera.z;
	pScreen.y = pCamera.y / -pCamera.z;
	pNDC.x = (float)(pScreen.x + canvasWidth * 0.5) / canvasWidth;
	pNDC.y = (float)(pScreen.y + canvasHeight * 0.5) / canvasHeight;
	pRaster->x = (int)(pNDC.x * imageWidth);
	pRaster->y = (int)((1 - pNDC.y) * imageHeight);

	return TRUE;
}