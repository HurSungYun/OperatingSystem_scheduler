%define BOARD_TIZEN_Z3	tizen_tm1

Name: linux-3.10-sc7730
Summary: The Linux Kernel
Version: Tizen_sc7730_20150907_1_2edc4585
Release: 1
License: GPL
Group: System Environment/Kernel
Vendor: The Linux Community
URL: http://www.kernel.org
Source0: %{name}-%{version}.tar.gz

BuildRoot: %{_tmppath}/%{name}-%{PACKAGE_VERSION}-root
Provides: linux-3.10
%define __spec_install_post /usr/lib/rpm/brp-compress || :
%define debug_package %{nil}

BuildRequires:  lzop
BuildRequires:  binutils-devel
BuildRequires:  module-init-tools
BuildRequires:	python
BuildRequires:	gcc
BuildRequires:	bash
BuildRequires:	system-tools
BuildRequires:	bc
ExclusiveArch:  %arm

%if "%{tizen_target_name}" != "Z300H"
ExcludeArch: %{arm}
%endif

%description
The Linux Kernel, the operating system core itself

%define BOARDS %{BOARD_TIZEN_Z3}

%{lua:
for targets in string.gmatch(rpm.expand("%{BOARDS}"), "[%w_-]+")
do
print("%package -n linux-3.10-sc7730_"..targets.." \n")
print("License:        TO_BE_FILLED \n")
print("Summary:        Linux support headers for userspace development \n")
print("Group:          TO_BE_FILLED/TO_BE_FILLED \n")
print("Requires(post): coreutils \n")
print("\n")
print("%files -n linux-3.10-sc7730_"..targets.." \n")
print("/boot/kernel/mod_"..targets.." \n")
print("/var/tmp/kernel/kernel-"..targets.."/dzImage \n")
print("/var/tmp/kernel/kernel-"..targets.."/dzImage-recovery \n")
print("\n")
print("%post -n linux-3.10-sc7730_"..targets.." \n")
print("cp -r /boot/kernel/mod_"..targets.."/lib/modules/* /lib/modules/. \n")
print("mv /var/tmp/kernel/kernel-"..targets.."/dzImage /var/tmp/kernel/. \n")
print("mv /var/tmp/kernel/kernel-"..targets.."/dzImage-recovery /var/tmp/kernel/. \n")
print("\n")
print("%description -n linux-3.10-sc7730_"..targets.." \n")
print("This package provides the sc7730_eur linux kernel image & module.img. \n")
print("\n")
print("%package -n linux-3.10-sc7730_"..targets.."-debuginfo \n")
print("License:        TO_BE_FILLED \n")
print("Summary:        Linux support headers for userspace development \n")
print("Group:          TO_BE_FILLED/TO_BE_FILLED \n")
print("\n")
print("%files -n linux-3.10-sc7730_"..targets.."-debuginfo \n")
print("/boot/kernel/mod_"..targets.." \n")
print("/var/tmp/kernel/kernel-"..targets.." \n")
print("\n")
print("%description -n linux-3.10-sc7730_"..targets.."-debuginfo \n")
print("This package provides the sc7730_eur linux kernel's debugging files. \n")
end }

%package -n kernel-headers-3.10-sc7730
License:        TO_BE_FILLED
Summary:        Linux support headers for userspace development
Group:          TO_BE_FILLED/TO_BE_FILLED
Provides:       kernel-headers, kernel-headers-tizen-dev
Obsoletes:      kernel-headers

%description -n kernel-headers-3.10-sc7730
This package provides userspaces headers from the Linux kernel.  These
headers are used by the installed headers for GNU glibc and other system
 libraries.

%package -n kernel-devel-3.10-sc7730
License:        GPL
Summary:        Linux support kernel map and etc for other package
Group:          System/Kernel
Provides:       kernel-devel-tizen-dev

%description -n kernel-devel-3.10-sc7730
This package provides kernel map and etc information.

%package -n linux-kernel-license
License:        GPL
Summary:        Linux support kernel license file
Group:          System/Kernel

%description -n linux-kernel-license
This package provides kernel license file.

%prep
%setup -q

%build
%if 0%{?tizen_build_binary_release_type_eng}
%define RELEASE_TYPE ENG
%else
%define RELEASE_TYPE USR
%endif

for i in %{BOARDS}; do
	target=$i
	mkdir -p %_builddir/mod_$target
	make distclean

	./release_obs.sh $target %{RELEASE_TYPE}

	cp -f arch/arm/boot/zImage %_builddir/zImage.$target
	cp -f arch/arm/boot/dzImage %_builddir/dzImage.$target
	cp -f arch/arm/boot/dzImage %_builddir/dzImage-recovery.$target
	cp -f System.map %_builddir/System.map.$target
	cp -f .config %_builddir/config.$target
	cp -f vmlinux %_builddir/vmlinux.$target
	make modules
	make modules_install INSTALL_MOD_PATH=%_builddir/mod_$target

	# prepare for devel package
	find %{_builddir}/%{name}-%{version} -name ".tmp_vmlinux*" -exec rm -f {} \;
	find %{_builddir}/%{name}-%{version} -name "\.*dtb*tmp" -exec rm -f {} \;
	find %{_builddir}/%{name}-%{version} -name "*\.*tmp" -exec rm -f {} \;
	find %{_builddir}/%{name}-%{version} -name "vmlinux" -exec rm -f {} \;
	find %{_builddir}/%{name}-%{version} -name "bzImage" -exec rm -f {} \;
	find %{_builddir}/%{name}-%{version} -name "zImage" -exec rm -f {} \;
	find %{_builddir}/%{name}-%{version} -name "dzImage" -exec rm -f {} \;
	find %{_builddir}/%{name}-%{version} -name "*.cmd" -exec rm -f {} \;
	find %{_builddir}/%{name}-%{version} -name "*\.ko" -exec rm -f {} \;
	find %{_builddir}/%{name}-%{version} -name "*\.o" -exec rm -f {} \;
	find %{_builddir}/%{name}-%{version} -name "*\.S" -exec rm -f {} \;
	find %{_builddir}/%{name}-%{version} -name "*\.c" -not -path "%{_builddir}/%{name}-%{version}/scripts/*" -exec rm -f {} \;

	# prepare for the next build
	cd %_builddir
	mv %{name}-%{version} kernel-devel-$target
	/bin/tar -xf %{SOURCE0}
	cd %{name}-%{version}
done

%install
mkdir -p %{buildroot}/usr
make mrproper
make headers_check
make headers_install INSTALL_HDR_PATH=%{buildroot}/usr

find  %{buildroot}/usr/include -name ".install" | xargs rm -f
find  %{buildroot}/usr/include -name "..install.cmd" | xargs rm -f
rm -rf %{buildroot}/usr/include/scsi
rm -f %{buildroot}/usr/include/asm*/atomic.h
rm -f %{buildroot}/usr/include/asm*/io.h

mkdir -p %{buildroot}/usr/share/license
cp -vf COPYING %{buildroot}/usr/share/license/linux-kernel

mkdir -p %{buildroot}/var/tmp/kernel/devel

for i in %{BOARDS}; do
	target=$i

	mkdir -p %{buildroot}/var/tmp/kernel/kernel-$i
	mkdir -p %{buildroot}/boot/kernel/

	mv %_builddir/mod_$target %{buildroot}/boot/kernel/mod_$i

	mv %_builddir/zImage.$target %{buildroot}/var/tmp/kernel/kernel-$i/zImage
	mv %_builddir/dzImage.$target %{buildroot}/var/tmp/kernel/kernel-$i/dzImage
	mv %_builddir/dzImage-recovery.$target %{buildroot}/var/tmp/kernel/kernel-$i/dzImage-recovery

	mv %_builddir/System.map.$target %{buildroot}/var/tmp/kernel/kernel-$i/System.map
	mv %_builddir/config.$target %{buildroot}/var/tmp/kernel/kernel-$i/config
	mv %_builddir/vmlinux.$target %{buildroot}/var/tmp/kernel/kernel-$i/vmlinux

	mv %_builddir/kernel-devel-$target %{buildroot}/var/tmp/kernel/devel/kernel-devel-$i
done

find %{buildroot}/var/tmp/kernel/ -name 'System.map' -not -path "%{buildroot}/var/tmp/kernel/devel/*" > develfiles.pre # for secure storage
find %{buildroot}/var/tmp/kernel/ -name 'vmlinux' -not -path "%{buildroot}/var/tmp/kernel/devel/*" >> develfiles.pre   # for TIMA
find %{buildroot}/var/tmp/kernel/ -name '*.ko' -not -path "%{buildroot}/var/tmp/kernel/devel/*" >> develfiles.pre      # for TIMA
cat develfiles.pre | sed -e "s#%{buildroot}##g" | uniq | sort > develfiles

%clean
rm -rf %_builddir

%files -n kernel-headers-3.10-sc7730
/usr/include/*

%files -n linux-kernel-license
/usr/share/license/*

%files -n kernel-devel-3.10-sc7730 -f develfiles
/var/tmp/kernel/devel/*
