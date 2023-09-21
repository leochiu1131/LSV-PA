make
./abc -c "read mul.blif; collapse -r; lsv_sim_bdd 0110;"
./abc -c "read mul.blif; collapse -r; lsv_sim_bdd 1111;"
