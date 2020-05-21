.PHONY: clean

boot_new.img: /mnt/d/gitlab/android_kernel_oneplus_msm8996/arch/arm64/boot/Image.gz-dtb ./abootimg/abootimg msm8996_pro/bootimg.cfg
	./abootimg/abootimg --create boot_new.img -f msm8996_pro/bootimg.cfg -k /mnt/d/gitlab/android_kernel_oneplus_msm8996/arch/arm64/boot/Image.gz-dtb -r msm8996_pro/initrd.img

./abootimg/abootimg:
	git clone https://github.com/ggrandou/abootimg.git
	(cd abootimg && make)

msm8996_pro/bootimg.cfg: ./abootimg/abootimg boot.img
	mkdir msm8996_pro || true
	(cd msm8996_pro && ../abootimg/abootimg -x ../boot.img)

clean:
	rm -rf abootimg || true
	rm boot_new.img || true


