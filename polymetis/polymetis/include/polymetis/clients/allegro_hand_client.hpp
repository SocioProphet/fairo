// Copyright (c) Facebook, Inc. and its affiliates.

// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.
#include "spdlog/spdlog.h"
#include <array>
#include <assert.h>
#include <chrono>
#include <memory>
#include <thread>

#include "yaml-cpp/yaml.h"

#include "polymetis/utils.h"
#include <grpcpp/grpcpp.h>

#define NUM_DOFS 16
#define UPDATE_HZ 300.0
#define RECOVERY_WAIT_SECS 1
#define RECOVERY_MAX_TRIES 3

class AllegroHand;
struct AllegroHandState;

class AllegroHandTorqueControlClient {
private:
    void updateServerCommand(const AllegroHandState &robot_state,
                             std::array<double, NUM_DOFS> &torque_out);

  // gRPC
  std::unique_ptr<PolymetisControllerServer::Stub> stub_;
  grpc::Status status_;

  TorqueCommand torque_command_;
  RobotState robot_state_;

  // Allegro Driver
  std::unique_ptr<AllegroHand> allegro_hand_ptr_;
  bool mock_allegro_;
  bool readonly_mode_;

  std::array<double, NUM_DOFS> torque_commanded_, torque_safety_, torque_applied_;
  double k_joint_pos_, k_joint_vel_;

  // limits
  bool limits_exceeded_;
  std::string error_str_;

  std::array<double, NUM_DOFS> joint_pos_ulimits_, joint_pos_llimits_,
      joint_vel_limits_, joint_torques_limits_;

public:
  /**
  TODO
  */
  AllegroHandTorqueControlClient(std::shared_ptr<grpc::Channel> channel,
                            YAML::Node config);
  void run();
};
