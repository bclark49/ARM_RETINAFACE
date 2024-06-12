import matplotlib.pyplot as mpl
import numpy as np

def extract_ints (filename):
    data = np.loadtxt(filename, delimiter=",", dtype=np.int64)
    tss = data[:,0]
    sizes = data[:,1]
    return tss, sizes

def main ():
    #client_tss, client_sizes = extract_ints ("late_client.txt")
    #server_tss, server_sizes = extract_ints ("late_server.txt")
    client_tss, client_sizes = extract_ints ("late_client.txt")
    server_tss, server_sizes = extract_ints ("late_server.txt")

    if ( len (client_tss) != len (server_tss) ):
        print ("ERROR: Files do not have the same number of integers")
        return

    total_trans_time = (server_tss[-1] - client_tss[0]) / 1000000

    diffs = (server_tss - client_tss)

    running_sum = np.cumsum (server_sizes)

    running_sum_Mb = running_sum / 1000000

    avg_lat = np.mean (diffs)
    min_lat = np.min (diffs)
    max_lat = np.max (diffs)
    std_dev = np.std (diffs)
    avg_br = (running_sum[-1] / 1000000) / total_trans_time

    print(f'Transmission sent {running_sum[-1]/1000000} Mb in {total_trans_time} s')
    print(f'Average Latency: {avg_lat} us')
    print(f'Min Latency: {min_lat} us')
    print(f'Max Latency: {max_lat} us')
    print(f'Standard Deviation: {std_dev} us')
    print(f'Average Bitrate: {avg_br} Mb/s')

    mpl.plot (running_sum_Mb, diffs, marker='o')
    mpl.xlabel("Running Total of data received (Mb)")
    mpl.ylabel("Latency per transfer (us)")
    mpl.title("Latency of video transmission")
    mpl.grid(True)
    mpl.show()

if __name__ == "__main__":
    main()

