out_file_name=sender/test.txt
file_size=$2
block_size=$1
tmp_out_file_name=$out_file_name.tmp

 
function check_file_size_if_integer()
{
    if [ -n "$file_size" -a "$file_size" = "${file_size//[^0-9]/}" ]; then
        echo "file_size=$file_size"
    else
        echo "[file-size] error: The file size of output file, which must be an integer."
    exit
    fi
}
 
function create_random_file()
{
    dd if=/dev/urandom of=$tmp_out_file_name bs=$block_size count=$file_size conv=notrunc
    mv $tmp_out_file_name $out_file_name
}


check_file_size_if_integer
create_random_file