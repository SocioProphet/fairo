"""polymetis.RobotInterface combined with GripperInterface, with an additional `grasp` method."""

import time
import numpy as np
from scipy.spatial.transform import Rotation as R
import torch
import open3d as o3d

import hydra
import graspnetAPI
import polymetis


def compute_des_pose(best_grasp):
    grasp_point = best_grasp.translation

    grasp_approach_delta = best_grasp.rotation_matrix @ np.array([-0.3, 0.0, 0])
    grasp_approach_delta_plus = best_grasp.rotation_matrix @ np.array([-0.3, 0.1, 0])
    grasp_approach_delta_minus = best_grasp.rotation_matrix @ np.array([-0.3, -0.1, 0])
    bx = -grasp_approach_delta
    by = grasp_approach_delta_plus - grasp_approach_delta_minus
    bx = bx / np.linalg.norm(bx)
    by = by / np.linalg.norm(by)
    bz = np.cross(bx, by)
    plane_rot = R.from_matrix(np.vstack([bx, by, bz]).T)

    des_ori = plane_rot * R.from_euler("x", 90, degrees=True) * R.from_euler("y", 90, degrees=True)
    des_ori_quat = des_ori.as_quat()

    return grasp_point, grasp_approach_delta, des_ori_quat


def grasp_to_pose(grasp: graspnetAPI.Grasp):
    return grasp.translation, R.from_matrix(grasp.rotation_matrix).as_quat()


def compute_quat_dist(a, b):
    return torch.acos((2*(a * b).sum()**2 - 1).clip(-1, 1))

class GraspingRobotInterface(polymetis.RobotInterface):
    def __init__(self, gripper: polymetis.GripperInterface, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.gripper = hydra.utils.instantiate(gripper)

        self.default_ee_pose = torch.Tensor([ 0.9418,  0.3289, -0.0368, -0.0592])

    def gripper_open(self):
        self.gripper.goto(1, 1, 1)

    def gripper_close(self):
        self.gripper.goto(0, 1, 1)

    def _move_until_success(self, position, orientation, time_to_go, max_attempts=5):
        states = []
        for _ in range(max_attempts):
            states += self.move_to_ee_pose(
                position=position, orientation=orientation, time_to_go=time_to_go
            )
            curr_ee_pos, curr_ee_ori = self.get_ee_pose()

            print(f"Dist to goal: {torch.linalg.norm(curr_ee_pos - position)}")

            if states[-1].prev_command_successful and torch.linalg.norm(curr_ee_pos - position) < 0.2:  # TODO: orientation diff
                break
        return states

    def select_grasp(
        self, grasps: graspnetAPI.GraspGroup, scene_pcd: o3d.geometry.PointCloud
    ) -> graspnetAPI.Grasp:
        # TODO: do something smarter than this
        grasps = grasps[:5]
        with torch.no_grad():
            rots_as_quat = [torch.Tensor(R.from_matrix(grasp.rotation_matrix).as_quat()) for grasp in grasps]
            dists = [compute_quat_dist(rot, self.default_ee_pose) for rot in rots_as_quat]
            i = torch.argmin(torch.Tensor(dists)).item()
            print(f"Grasp {i} has angle {dists[i]} from neutral")
        return grasps[i]

        # print(f"Closest grasp to ee ori, within top 5: {i + 1}")
        # return grasps[int(input("Choose grasp index (1-indexed):")) - 1]

        # return grasps[0]

    def grasp(self, grasp: graspnetAPI.Grasp):
        time_to_go = 2
        states = []
        grasp_point, grasp_approach_delta, des_ori_quat = compute_des_pose(grasp)

        offset = np.array([0.1, 0.0, 0.1])

        self.gripper_open()
        states += self._move_until_success(
            position=torch.Tensor(grasp_point + grasp_approach_delta * 1.5 + offset),
            orientation=torch.Tensor(des_ori_quat),
            time_to_go=time_to_go,
        )

        grip_ee_pos = torch.Tensor(grasp_point + grasp_approach_delta * 0.75 + offset)

        states += self._move_until_success(
            position=grip_ee_pos, orientation=torch.Tensor(des_ori_quat), time_to_go=time_to_go
        )
        self.gripper_close()

        print(f"Waiting for gripper to close...")
        time.sleep(1.5)

        gripper_state = self.gripper.get_state()
        width = gripper_state.width
        print(f"Gripper width: {width}")

        success = width > 0.0005

        return states, success
