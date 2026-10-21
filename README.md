# DiskMark
DiskMark is a small utility that allows you to benchmark your disks and hard drives performance. It allows you to specify many advanced options to determine how your drives perform best and to determine the typical operating performance.


![image](https://github.com/NTDLS/DiskMark/assets/11428567/0c663d76-56ba-4dbb-a420-53578845bcde)


## How the numbers are measured
- The test writes sequential `Block size` I/Os to a temporary `Test file size` file (created on the selected drive and deleted afterwards), wrapping around the file, for `Duration` seconds; then reads it back for the same time. Throughput is sampled once per second.
- **Average** is total bytes / total time (not a mean of per-sample rates). **Min/Max/Last** are per-second-sample throughput. **IOPS** and **Latency** are per-block averages. 1 MB = 1,048,576 bytes.
- Writes use `FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH` unless *Allow OS write caching* is ticked, in which case the file flush is included in the timed region so the disk, not RAM, is measured.
- Reads are always unbuffered, so they come from the device rather than the Windows file cache. Block size is rounded up to the volume's sector size.
- The I/O buffer is page aligned and filled with incompressible pseudo-random data.
- If the drive root is not writable (non-admin), the test file is placed in a `DiskMark-Temp` folder on that drive.
- Validated against the Windows `PhysicalDisk` performance counters: a 20 GB-per-phase run reported 2641 MB/s write / 2205 MB/s read versus roughly 2650 / 2240 MB/s from the counters.
