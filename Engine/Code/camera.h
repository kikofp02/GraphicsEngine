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

const float DEFAULT_YAW = -90.0f;
//const float DEFAULT_PITCH = 0.0f;
const float DEFAULT_SPEED = 15.f;
const float DEFAULT_SENSITIVITY = 0.1f;
const float DEFAULT_ZOOM = 45.0f;

const float Z_NEAR = 1.0f;
const float Z_FAR = 500.f;

class Camera {
public:
    // Camera attributes
    glm::vec3 Position;
    glm::vec3 Front;
    glm::vec3 Up;
    glm::vec3 Right;
    glm::vec3 WorldUp;

    // Euler angles
    float Yaw;
    float Pitch;

    // Camera options
    float MovementSpeed;
    float MouseSensitivity;
    float Zoom;

    float z_near;
    float z_far;

    // Constructors
    Camera(glm::vec3 position = glm::vec3(0.0f),
        glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f),
        float yaw = DEFAULT_YAW,
        float pitch = 0.0f)
        : Front(glm::vec3(0.0f, 0.0f, -1.0f)),
        MovementSpeed(DEFAULT_SPEED),
        MouseSensitivity(DEFAULT_SENSITIVITY),
        Zoom(DEFAULT_ZOOM)
    {
        Position = position;
        WorldUp = up;
        Yaw = yaw;
        Pitch = pitch;

        z_near = Z_NEAR;
        z_far = Z_FAR;

        UpdateVectors();
    }

    // Get view matrix
    inline glm::mat4 GetViewMatrix() const {
        return glm::lookAt(Position, Position + Front, Up);
    }

    // Process keyboard input
    inline void ProcessKeyboard(Movement direction, float deltaTime) {
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

    // Process mouse movement
    inline void ProcessMouseMovement(float xoffset, float yoffset, bool constrainPitch = true) {
        xoffset *= MouseSensitivity;
        yoffset *= MouseSensitivity;

        Yaw += xoffset;
        Pitch += yoffset;

        if (constrainPitch) {
            Pitch = glm::clamp(Pitch, -89.0f, 89.0f);
        }

        UpdateVectors();
    }

    // Process mouse scroll
    inline void ProcessMouseScroll(float yoffset) {
        Zoom -= yoffset;
        Zoom = glm::clamp(Zoom, 1.0f, 45.0f);
    }


    // Update camera vectors
    inline void UpdateVectors() {
        // Calculate new Front vector
        glm::vec3 front;
        front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        front.y = sin(glm::radians(Pitch));
        front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        Front = glm::normalize(front);

        // Re-calculate Right and Up vectors
        Right = glm::normalize(glm::cross(Front, WorldUp));
        Up = glm::normalize(glm::cross(Right, Front));
    }
};