// (c) Facebook, Inc. and its affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.
#include <memory.h>

#include <cmath>
#include <cstdio>
#include <utility>

#include "event_watcher.hpp"
#include "pcan_interface.hpp"

#include "allegro_hand.hpp"
#include "utility.hpp"

#define ALLEGRO_MSG_ID_SERVO_ON 0x40u
#define ALLEGRO_MSG_ID_SERVO_OFF 0x41

#define ALLEGRO_MSG_ID_INFORMATION 0x80u
#define ALLEGRO_MSG_ID_SERIAL 0x88u
#define ALLEGRO_MSG_ID_SET_PERIODIC_READ 0x81u

// WARNING: This value does not match the documentation (0x50).
#define ALLEGRO_MSG_ID_SET_TORQUE 0x60u
#define ALLEGRO_MSG_ID_POS1 0x20u
#define ALLEGRO_MSG_ID_POS2 0x21u
#define ALLEGRO_MSG_ID_POS3 0x22u
#define ALLEGRO_MSG_ID_POS4 0x23u

#define ALLEGRO_MSG_ID_TEMP1 0x38
#define ALLEGRO_MSG_ID_TEMP2 0x39
#define ALLEGRO_MSG_ID_TEMP3 0x3A
#define ALLEGRO_MSG_ID_TEMP4 0x3B

#define ALLEGRO_MSG_ID_IMU 0x30u
#define ALLEGRO_MSG_ID_STATUS 0x10u

constexpr double kTorqueMax = 1200.0;

struct AllegroInfo {
    WORD hw_version;
    WORD firmware_version;
    BYTE chirality;
    BYTE temp;
    BYTE stat;
} __attribute__((__packed__));

AllegroHandImpl::AllegroHandImpl(PcanInterface&& pcan):pcan_(std::move(pcan)) {
    setPeriodicRead(3);

    for (int f = 0; f < kNFinger; f++) {
        send_pending_[f] = false;
    }

    memset(torques_, 0, sizeof(torques_));
    memset(position_, 0, sizeof(position_));
    memset(state_updated_, 0, sizeof(state_updated_));
}
void AllegroHandImpl::setPeriodicRead(uint16_t millis) {
    WORD periods[4];
    periods[0] = millis;
    periods[1] = 0;
    periods[2] = 0;
    periods[3] = 0;
    send(ALLEGRO_MSG_ID_SET_PERIODIC_READ, 8, periods);
}

void AllegroHandImpl::setServoEnable(bool enable) {
    if (enable) {
        send(ALLEGRO_MSG_ID_SERVO_ON);
    } else {
        send(ALLEGRO_MSG_ID_SERVO_OFF);
    }
}

void AllegroHandImpl::setTorques(double *torques) {
    for (int i = 0; i < kNDofs; i++) {
        VALIDATE(std::isfinite(torques[i]));
    }

    memcpy(torques_, torques, sizeof(double) * kNDofs);
}

void AllegroHandImpl::requestStatus() {
    send(ALLEGRO_MSG_ID_INFORMATION, 0, NULL, true);
}

int AllegroHandImpl::poll() {
    TPCANMsg msg;

    for (int f = 0; f < kNFinger; f++) {
        if (send_pending_[f]) {
            sendTorque(f);
            send_pending_[f] = false;
        }
    }

    if (pcan_.readPcan(&msg)) {
        int message_id = msg.ID >> 2;
        switch (message_id) {
            case ALLEGRO_MSG_ID_INFORMATION: {
                AllegroInfo *info = reinterpret_cast<AllegroInfo *>(msg.DATA);
                printf("Info: hw_ver: %u, firmware_ver:%u, chirality: %u, temp:%u, stat:%x\n",
                       info->hw_version, info->firmware_version, info->chirality, info->temp, info->stat);
                printf("Status: Servo:%s, Temp Fault:%s, Temp Throttle:%s, Comm Timeout:%s,  Palm Temp Fault:%s\n",
                       ((info->stat & (1 << 0)) != 0) ? "True" : "False",
                       ((info->stat & (1 << 1)) != 0) ? "True" : "False",
                       ((info->stat & (1 << 2)) != 0) ? "True" : "False",
                       ((info->stat & (1 << 3)) != 0) ? "True" : "False",
                       ((info->stat & (1 << 4)) != 0) ? "True" : "False");
            }
                break;
            case ALLEGRO_MSG_ID_SERIAL:
                printf("Serial: %.*s\n", msg.LEN, msg.DATA);
                break;
            case ALLEGRO_MSG_ID_POS1:
            case ALLEGRO_MSG_ID_POS2:
            case ALLEGRO_MSG_ID_POS3:
            case ALLEGRO_MSG_ID_POS4: {
                gEventWatcher.observe("joint_msg_recv");
                int finger = message_id & 0x07;
                send_pending_[finger] = true;
                int16_t* raw_pos = reinterpret_cast<int16_t *>(msg.DATA);
                for (int j = 0; j < kNJoint; j++) {
                    position_[finger * kNJoint + j] = convertPosition(raw_pos[j]);
                }
                state_updated_[finger] = true;
                return finger;
            }
            case ALLEGRO_MSG_ID_IMU:
                gEventWatcher.observe("imu_msg_recv");
                break;
            case ALLEGRO_MSG_ID_TEMP1:
            case ALLEGRO_MSG_ID_TEMP2:
            case ALLEGRO_MSG_ID_TEMP3:
            case ALLEGRO_MSG_ID_TEMP4:
                gEventWatcher.observe("temp_msg_recv");
                break;
            default:
                printf("Unknown ID: %x type:%x\n", message_id, msg.MSGTYPE);
                break;
        }
    } else {
        gEventWatcher.observe("no_msg_recv");
    }
    return -1;
}

bool AllegroHandImpl::allStatesUpdated() const {
    for (int f = 0; f < kNFinger; f++) {
        if (!state_updated_[f]) {
            return false;
        }
    }
    return true;
}

void AllegroHandImpl::sendTorque(int finger) {
    int16_t finger_torques[kNJoint];
    for (int finger = 0; finger < kNFinger; finger++) {
        for (int joint = 0; joint < kNJoint; joint++) {
            finger_torques[joint] = torques_[finger * kNJoint + joint] * kTorqueMax;
        }
        send(ALLEGRO_MSG_ID_SET_TORQUE + finger, 8, finger_torques);
    }
}

double *AllegroHandImpl::getPositions() {
    return position_;
}

bool AllegroHandImpl::send(unsigned char msg_id, char data_len, const void* data, bool expect_return) {
    TPCANMsg msg;
    msg.ID = msg_id << 2 | device_id_;
    msg.MSGTYPE = expect_return ? PCAN_MESSAGE_RTR : PCAN_MESSAGE_STANDARD;
    msg.LEN = data_len;
    if (data != nullptr) {
        memcpy(msg.DATA, data, data_len);
    }
    return pcan_.writePcan(msg);
}

inline double AllegroHandImpl::convertPosition(int16_t raw_pos) const {
    // return raw_pos * 2 * M_PI / (32767 * 330);  // Conversion constant per documentation.
    return raw_pos * (M_PI / 180) * (333.3/65536);  // WARNING: Matches example code, not docs.
}
