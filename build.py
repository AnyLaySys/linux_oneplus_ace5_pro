import argparse
import os
from pathlib import Path
import subprocess
ROOT=Path(__file__).resolve().parent
DEFCONFIG="oneplus_ace5_pro_defconfig"
DTB="qcom/sm8750-oneplus-hummer.dtb"
def main():
    parser=argparse.ArgumentParser(description="Build the OnePlus Ace 5 Pro Linux kernel, DTB and modules")
    parser.add_argument("--out",type=Path,default=ROOT/"out/kernel")
    parser.add_argument("-j","--jobs",type=int,default=min(os.cpu_count() or 1,12))
    args=parser.parse_args()
    if args.jobs<1:
        parser.error("--jobs must be positive")
    output=args.out.resolve()
    if not output.is_relative_to(ROOT/"out"):
        parser.error("--out must be inside this repository's out directory")
    output.mkdir(parents=True,exist_ok=True)
    environment=os.environ.copy()
    environment.setdefault("KBUILD_BUILD_USER","hummer")
    environment.setdefault("KBUILD_BUILD_HOST","builder")
    environment.setdefault("KBUILD_BUILD_TIMESTAMP","2026-10-09 00:00:00 +0000")
    environment.setdefault("KBUILD_BUILD_VERSION","1")
    command=["make","-C",str(ROOT),"O="+str(output),"ARCH=arm64","LLVM=1","LLVM_IAS=1","LOCALVERSION="]
    subprocess.run(command+["olddefconfig"],env=environment,check=True)
    subprocess.run(command+["-j"+str(args.jobs),"Image","modules",DTB],env=environment,check=True)
    print("Kernel:",output/"arch/arm64/boot/Image")
    print("DTB:",output/"arch/arm64/boot/dts"/DTB)
    print("Modules:",output/"modules.order")
if __name__=="__main__":
    main()