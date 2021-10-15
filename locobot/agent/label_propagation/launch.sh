source activate /private/home/apratik/.conda/envs/denv3

'''
./launch.sh <root folder with all trajectory data> <setting specific path> <num of trajectories> <num of training runs> <slurm or local>
Example commands to run this file
./launch.sh /checkpoint/apratik/data_devfair0187/apartment_0/straightline/no_noise/1633991019 apartment_0/straightline/no_noise 10 5
'''

data_path=$1
# Base dir for all jobs
base_dir=/checkpoint/${USER}/jobs/active_vision/pipeline/$2
mkdir -p $base_dir
dt=$(date '+%d-%m-%Y/%H:%M:%S');

jobdir=$base_dir/$dt
echo """"""""""""""""""""""""""""""
echo Job Directory $jobdir
mkdir -p $jobdir
echo """"""""""""""""""""""""""""""

codedir=$jobdir/code
mkdir -p $codedir

cp coco.py $codedir/coco.py
cp label_propagation.py $codedir/label_propagation.py
cp slurm_train.py $codedir/slurm_train.py
cp run_pipeline.py $codedir/run_pipeline.py

cd $codedir
chmod +x run_pipeline.py
python3.7 run_pipeline.py --data $data_path --job_folder $jobdir --num_traj $3 --num_train_samples $4 #--slurm --active