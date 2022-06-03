rm *.png # cleanup
# Parameters
metagraph_path="/home/alex/metagraph/metagraph/build/metagraph"
output_path="./data/generated.fa"
max_k=90
graph_seq_len=50000
############

# General params
num_query_seqs=1000
parallel=$(getconf _NPROCESSORS_ONLN)
mutation_rate=20
min_path_size=99
max_path_size=100
################

# Generate the dataset
gen_command="python generate_dataset.py --metagraph-path $metagraph_path --graph-seq-len $graph_seq_len --max-k $max_k"
eval $gen_command

# Sketch seeder params
embed_dim=8
n_times_sketch=1
batch_size=100000
seeder="sketch"
######################

sketch_command="python seed_recall_on_k.py --output-path $output_path --embed-dim $embed_dim --n-times-sketch $n_times_sketch --mutation_rate $mutation_rate --num_query_seqs $num_query_seqs --parallel $parallel --batch-size $batch_size --seeder $seeder --metagraph-path $metagraph_path --max-k $max_k --max-path-size $max_path_size --min-path-size $min_path_size"

echo "[INFO] Launching the experiment on SKETCH"
eval $sketch_command

# Default seeder params
seeder="default"
#######################

default_command="python seed_recall_on_k.py --output-path $output_path --num_query_seqs $num_query_seqs --mutation_rate $mutation_rate --metagraph-path $metagraph_path --max-k $max_k --batch-size $batch_size --seeder $seeder --max-path-size $max_path_size --min-path-size $min_path_size"

echo "[INFO] Launching the experiment on DEFAULT"
eval $default_command

# Do overlap plot
runs=($(ls runs | sort | tail -n 2))
sketch_run="./runs/${runs[0]}/points.json"
default_run="./runs/${runs[1]}/points.json"

overlap_command="python overlap.py --max-k $max_k --sketch-json $sketch_run --exact-json $default_run"

echo $sketch_run
echo $default_run
echo "[INFO] Saving overlap plot"

eval $overlap_command

echo "Finished experiment"