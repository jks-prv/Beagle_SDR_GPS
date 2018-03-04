#!/bin/sh -e

check_dpkg () {
	LC_ALL=C dpkg --list | awk '{print $2}' | grep "^${pkg}" >/dev/null || deb_pkgs="${deb_pkgs}${pkg} "
}

unset deb_pkgs
pkg="bison"
check_dpkg
pkg="build-essential"
check_dpkg
pkg="flex"
check_dpkg
pkg="git-core"
check_dpkg

if [ "${deb_pkgs}" ] ; then
	echo "Installing: ${deb_pkgs}"
	sudo apt-get update
	sudo apt-get -y install ${deb_pkgs}
	sudo apt-get clean
fi

#git_sha="origin/master"
git_sha="origin/dtc-v1.4.4"
project="dtc"
#server="https://git.kernel.org/pub/scm/utils/dtc"
server="https://github.com/RobertCNelson"

if [ ! -f ${HOME}/git/bb.org-${project}/.git/config ] ; then
	git clone ${server}/${project}.git ${HOME}/git/bb.org-${project}/
fi

if [ ! -f ${HOME}/git/bb.org-${project}/.git/config ] ; then
	rm -rf ${HOME}/git/bb.org-${project}/ || true
	echo "error: git failure, try re-runing"
	exit
fi

cd ${HOME}/git/bb.org-${project}/
make clean
git checkout master -f
git pull || true

test_for_branch=$(git branch --list ${git_sha}-build)
if [ "x${test_for_branch}" != "x" ] ; then
	git branch ${git_sha}-build -D
fi

git checkout ${git_sha} -b ${git_sha}-build

make clean
make PREFIX=/usr/local/ CC=gcc CROSS_COMPILE= all
echo "Installing into: /usr/local/bin/"
sudo make PREFIX=/usr/local/ install
sudo ln -sf /usr/local/bin/dtc /usr/bin/dtc
echo "dtc: `/usr/bin/dtc --version`"
