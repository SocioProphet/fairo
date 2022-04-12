"""
Connects to robot, camera(s), gripper, and grasp server.
Runs grasps generated from grasp server.
"""

import random
import json


if __name__ == "__main__":
    # Initialize robot & gripper
    robot = RobotInterface()
    gripper = GripperInterface()
    robot.init_gripper(gripper)

    # Initialize cameras
    cameras = RealsenseAPI()
    camera_intrinsics = cameras.get_intrinsics()
    camera_extrinsics = json.load("/path/to/calibration.json")

    # Connect to grasp candidate selection and pointcloud processor
    pcd_client = PointCloudClient(camera_intrinsics, camera_extrinsics)
    grasp_suggester = GraspInterface()

    num_iters = 1
    for i in range(num_iters):
        # Get RGBD & pointcloud
        rgbd = cameras.get_rgbd()
        scene_pcd = pcd_client.get_pcd(rgbd)

        # Get grasps per object
        obj_to_pcd = pcd_client.segment_pcd(scene_pcd)
        obj_to_grasps = {obj: grasp_suggester.get_grasps(pcd) for obj, pcd in obj_to_pcd.items()}

        # Pick a random object to grasp
        curr_obj, curr_grasps = random.choice(list(obj_to_grasps.items()))
        print(f"Picking object with ID {curr_obj}")

        # Choose a grasp for this object
        # TODO: scene-aware motion planning for grasps
        des_ee_pos, des_ee_ori = robot.select_grasp(curr_grasps, scene_pcd)

        # Execute grasp
        traj, success = robot.grasp(ee_pos=des_ee_pos, ee_ori=des_ee_ori)
        print(f"Grasp success: {success}")

        if success:
            print(f"Moving end-effector up and down")
            curr_pose, curr_ori = robot.get_ee_pose()
            robot.move_to_ee_pose(torch.Tensor([0, 0, 0.1]), delta=True)
            robot.move_to_ee_pose(torch.Tensor([0, 0, -0.1]), delta=True)
