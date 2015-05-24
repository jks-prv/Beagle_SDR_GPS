# ~/.profile: executed by Bourne-compatible login shells.

if [ -f ~/.bashrc ]; then
  . ~/.bashrc
fi

# path set by /etc/profile
# export PATH

export PATH=.:$PATH
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib

export LS_OPTIONS='--color=auto'
