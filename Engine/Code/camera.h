// camera.h
#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

enum Movement {
    M_FORWARD,
    M_BACKWARD,
    M_LEFT,
    M_RIGHT,
    M_UP,
    M_DOWN
};

enum CameraMode {
    CAMERA_FREE,
    CAMERA_ORBIT
};

const float DEFAULT_YAW = -90.0f;
//const float DEFAULT_PITCH = 0.0f;
const float DEFAULT_SPEED = 15.f;
const float DEFAULT_SENSITIVITY = 0.1f;
const float DEFAULT_ZOOM = 45.0f;
const float DEFAULT_ORBIT_SPEED = 2.5f;
const float DEFAULT_ORBIT_DISTANCE = 8.0f;

const float Z_NEAR = 1.0f;
const float Z_FAR = 500.f;

class Camera {
public:
    glm::vec3 Position;
    glm::vec3 Front;
    glm::vec3 Up;
    glm::vec3 Right;
    glm::vec3 WorldUp;

    glm::vec3 OrbitTarget;
    float OrbitDistance;
    float OrbitAngleX;
    float OrbitAngleY;
    float OrbitSpeed;

    float Yaw;
    float Pitch;

    float MovementSpeed;
    float MouseSensitivity;
    float Zoom;

    float z_near;
    float z_far;

    CameraMode Mode;

    Camera(glm::vec3 position = glm::vec3(0.0f),
        glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f),
        float yaw = DEFAULT_YAW,
        float pitch = 0.0f)
        : Front(glm::vec3(0.0f, 0.0f, -1.0f)),
        MovementSpeed(DEFAULT_SPEED),
        MouseSensitivity(DEFAULT_SENSITIVITY),
        Zoom(DEFAULT_ZOOM),
        OrbitSpeed(DEFAULT_ORBIT_SPEED),
        OrbitDistance(DEFAULT_ORBIT_DISTANCE),
        OrbitAngleX(0.0f),
        OrbitAngleY(0.0f),
        Mode(CAMERA_FREE)
    {
        Position = position;
        WorldUp = up;
        Yaw = yaw;
        Pitch = pitch;

        z_near = Z_NEAR;
        z_far = Z_FAR;

        OrbitTarget = glm::vec3(0.0f);

        UpdateVectors();
    }

    inline glm::mat4 GetViewMatrix() const {
        if (Mode == CAMERA_ORBIT) {
            return glm::lookAt(Position, OrbitTarget, Up);
        }
        return glm::lookAt(Position, Position + Front, Up);
    }

    inline void ProcessKeyboard(Movement direction, float deltaTime) {
        if (Mode == CAMERA_ORBIT) return;

        float velocity = MovementSpeed * deltaTime;
        switch (direction) {
        case M_FORWARD:  Position += Front * velocity; break;
        case M_BACKWARD: Position -= Front * velocity; break;
        case M_LEFT:     Position -= Right * velocity; break;
        case M_RIGHT:    Position += Right * velocity; break;
        case M_UP:       Position += Up * velocity; break;
        case M_DOWN:     Position -= Up * velocity; break;
        }
    }

    inline void ProcessMouseMovement(float xoffset, float yoffset, bool constrainPitch = true) {
        xoffset *= MouseSensitivity;
        yoffset *= MouseSensitivity;

        if (Mode == CAMERA_ORBIT) {
            OrbitAngleX += xoffset * OrbitSpeed;
            OrbitAngleY += yoffset * OrbitSpeed;

            OrbitAngleY = glm::clamp(OrbitAngleY, -89.0f, 89.0f);

            UpdateOrbitPosition();
        }
        else {
            Yaw += xoffset;
            Pitch += yoffset;

            if (constrainPitch) {
                Pitch = glm::clamp(Pitch, -89.0f, 89.0f);
            }
            UpdateVectors();
        }
    }

    inline void ProcessMouseScroll(float yoffset) {
        if (Mode == CAMERA_ORBIT) {
            OrbitDistance -= yoffset;
            OrbitDistance = glm::clamp(OrbitDistance, 1.0f, 100.0f);
            UpdateOrbitPosition();
        }
        else {
            Zoom -= yoffset;
            Zoom = glm::clamp(Zoom, 1.0f, 45.0f);
        }
    }

    inline void SetMode(CameraMode mode) {
        if (Mode == mode) return;
        
        if (mode == CAMERA_ORBIT) {
            //OrbitDistance = glm::distance(Position, OrbitTarget);
            glm::vec3 dir = glm::normalize(Position - OrbitTarget);
            OrbitAngleY = glm::degrees(asin(dir.y));
            OrbitAngleX = glm::degrees(atan2(dir.z, dir.x));
            UpdateOrbitPosition();
        }
        else {
            Yaw = glm::degrees(atan2(Front.z, Front.x));
            Pitch = glm::degrees(asin(Front.y));

            UpdateVectors();
        }

        Mode = mode;
    }

    inline void SetOrbitTarget(const glm::vec3& target) {
        OrbitTarget = target;
        if (Mode == CAMERA_ORBIT) {
            UpdateOrbitPosition();
        }
    }

    inline void UpdateVectors() {
        glm::vec3 front;
        front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        front.y = sin(glm::radians(Pitch));
        front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        Front = glm::normalize(front);

        Right = glm::normalize(glm::cross(Front, WorldUp));
        Up = glm::normalize(glm::cross(Right, Front));
    }

    inline void UpdateOrbitPosition() {
        Position.x = OrbitTarget.x + OrbitDistance * cos(glm::radians(OrbitAngleX)) * cos(glm::radians(OrbitAngleY));
        Position.y = OrbitTarget.y + OrbitDistance * sin(glm::radians(OrbitAngleY));
        Position.z = OrbitTarget.z + OrbitDistance * sin(glm::radians(OrbitAngleX)) * cos(glm::radians(OrbitAngleY));

        Front = glm::normalize(OrbitTarget - Position);
        Right = glm::normalize(glm::cross(Front, WorldUp));
        Up = glm::normalize(glm::cross(Right, Front));
    }
};