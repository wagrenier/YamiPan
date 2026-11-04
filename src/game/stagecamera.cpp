/* ==========================================================================
 *  game/stagecamera.cpp
 *
 *  The stage's camera base (vtable 0x311860), as much of it as the first 3D
 *  picture needs: the view-screen matrix and the view from the object's own
 *  matrix. The file name is the port's. Its step, 0x178160, is not written:
 *  until then a camera stays where its object was placed, so the host hook
 *  below (YAMIPAN_CAMERA) puts it somewhere to look at a map from.
 * ======================================================================== */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "game/gamehooks.h"
#include "game/vu0math.h"

constexpr float kCameraScreenCenter = 2048.0f;
constexpr float kCameraZMin = 1.0f;
constexpr float kCameraZMax = 16777215.0f;
constexpr float kCameraNear = 1.0f;
constexpr float kHalfPi = 1.5707964f;       // 0x3fc90fdb
constexpr float kDisplayShape = 3.0f / 4.0f;

// FS2 0x00177d60
// callcheck: StageObject, identityMatrix, rotateMatrixX, sceVu0ViewScreenMatrix, sceVu0MulMatrix

StageCamera::StageCamera(float scrZ, float width, float height, float farZ)
{
    sceVu0FMATRIX axes;
    identityMatrix(axes);
    rotateMatrixX(kHalfPi, axes, axes);

    m_scrZ = scrZ;
    m_screenWidth = width;
    m_screenHeight = height;
    m_range = farZ;
    sceVu0ViewScreenMatrix(m_screen, m_scrZ, 1.0f, m_screenHeight / ((m_screenWidth * 3.0f) / 4.0f),
                           kCameraScreenCenter, kCameraScreenCenter, kCameraZMin, kCameraZMax, kCameraNear, m_range);
    sceVu0MulMatrix(m_screen, m_screen, axes);
    m_range2 = 0.0f;
}

// PC: until the camera's step is written (file header) YAMIPAN_CAMERA can
// place it: "eye x,y,z,target x,y,z" with z up, or "auto", which rena's walk
// answers by aiming it at the map (rena_draw.cpp).
static bool  sDebugSet = false;
static float sDebugEye[3];
static float sDebugTarget[3];
static float sDebugUp[3] = { 0.0f, 0.0f, 1.0f };

void stageCameraDebugLookAt(const float *eye, const float *target, const float *up)
{
    for (int i = 0; i < 3; i++) {
        sDebugEye[i] = eye[i];
        sDebugTarget[i] = target[i];
        sDebugUp[i] = up[i];
    }
    sDebugSet = true;
}

static void cross(float *out, const float *a, const float *b)
{
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}

static void normalize(float *v)
{
    const float length = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    v[0] /= length;
    v[1] /= length;
    v[2] /= length;
}

// The axes are the object's: x right, y ahead, z up. False without a place.
static bool debugCameraMatrix(sceVu0FMATRIX out)
{
    static bool sRead = false;
    if (!sRead) {
        sRead = true;
        const char *text = getenv("YAMIPAN_CAMERA");
        float values[6];
        if (text != nullptr
            && sscanf(text, "%f,%f,%f,%f,%f,%f", &values[0], &values[1], &values[2], &values[3], &values[4],
                      &values[5]) == 6) {
            const float up[3] = { 0.0f, 0.0f, 1.0f };
            stageCameraDebugLookAt(values, values + 3, up);
        }
    }
    if (!sDebugSet)
        return false;

    float ahead[3] = { sDebugTarget[0] - sDebugEye[0], sDebugTarget[1] - sDebugEye[1], sDebugTarget[2] - sDebugEye[2] };
    normalize(ahead);
    float right[3];
    cross(right, ahead, sDebugUp);
    normalize(right);
    float up[3];
    cross(up, right, ahead);
    for (int i = 0; i < 3; i++) {
        out[0][i] = right[i];
        out[1][i] = ahead[i];
        out[2][i] = up[i];
        out[3][i] = sDebugEye[i];
    }
    out[0][3] = out[1][3] = out[2][3] = 0.0f;
    out[3][3] = 1.0f;
    return true;
}

// FS2 0x00177810
// callcheck: invertRigidMatrix, multiplyMatrix

void StageCamera::calcViewMatrix()
{
    sceVu0FMATRIX debug;
    if (debugCameraMatrix(debug))
        invertRigidMatrix(m_view, debug);
    else
        invertRigidMatrix(m_view, getMatrix());
    multiplyMatrix(m_viewScreen, m_screen, m_view);
}
