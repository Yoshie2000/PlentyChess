from concurrent.futures import ThreadPoolExecutor
import os
import shutil
import subprocess
import sys
import glob

if (len(sys.argv) < 2):
    print("Please provide the path to the folder of bullet data")
    exit(-1)

dataFolder = sys.argv[1]

def rescoreFile(filename):
    testId = filename.split(".")[0]
    if os.path.exists(f"{dataFolder}/{testId}r.data"):
        return

    print(f"Copying {testId}")

    if not os.path.exists(f"./data/{filename}"):
        shutil.copyfile(f"{dataFolder}/{filename}", f"./data/{filename}")

    print(f"Rescoring {testId}")

    subprocess.call(f"./engine rescore data/{filename}", shell=True)

    # {testId}r.data, maybe {testId}rX.data => concatenate => {testId}rc.data
    concatFiles = sorted(glob.glob(f"./data/{testId}r*.data"))
    with open(f"./data/{testId}rc.data", "wb") as outf:
        for concatFile in concatFiles:
            with open(concatFile, "rb") as inf:
                outf.write(inf.read())

    # shutil.copyfile(f"./data/{testId}rc.data", f"{dataFolder}/{testId}r.data")
    # os.remove(f"./data/{testId}.data")
    # for concatFile in concatFiles:
        # os.remove(concatFile)
    # os.remove(f"./data/{testId}rc.data")

    # bytes_to_copy = os.path.getsize(f"./data/{testId}r.data")
    # with open(f"./data/{testId}.data", "rb") as infile:
    #     with open(f"./data/{testId}s.data", "wb") as outfile:
    #         data = infile.read(bytes_to_copy)
    #         outfile.write(data)


files = [filename for filename in os.listdir(dataFolder) if not "adju" in filename and not "r" in filename and filename in ["3173.data", "3149.data", "3134.data", "3119.data", "3084.data", "3071.data"]]
with ThreadPoolExecutor(max_workers=6) as executor:
    executor.map(lambda f: rescoreFile(f), files)