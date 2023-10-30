_cond()
{
    initctl -pt cond dump |  awk '{print $4}' | sed 's/<\(.*\)>/\1/'
}

_ident()
{
    if [ -n "$1" ]; then
	initctl ident | grep -q $1
    else
	initctl ident
    fi
}

_svc()
{
    initctl ls -pt | grep $1 | sed "s/.*\/\(.*\)/\1/g" | sort -u
}

_enabled()
{
    echo "$(_svc enabled)"
}

_available()
{
    all=$(mktemp)
    ena=$(mktemp)
    echo "$(_svc available)" >$all
    echo "$(_svc enabled)"   >$ena
    grep -v -f $ena $all
    rm $all $ena
}

# Determine first non-option word. Usually the command
_firstword() {
	local firstword i

	firstword=
	for ((i = 1; i < ${#COMP_WORDS[@]}; ++i)); do
		if [[ ${COMP_WORDS[i]} != -* ]]; then
			firstword=${COMP_WORDS[i]}
			break
		fi
	done

	echo $firstword
}

# Determine last non-option word. Uusally a sub-command
_lastword() {
	local lastword i

	lastword=
	for ((i = 1; i < ${#COMP_WORDS[@]}; ++i)); do
		if [[ ${COMP_WORDS[i]} != -* ]] && [[ -n ${COMP_WORDS[i]} ]] && [[ ${COMP_WORDS[i]} != $cur ]]; then
			lastword=${COMP_WORDS[i]}
		fi
	done

	echo $lastword
}

_initctl()
{
    local cur command

    cur=${COMP_WORDS[COMP_CWORD]}
    prev="${COMP_WORDS[COMP_CWORD-1]}"
    firstword=$(_firstword)
    lastword=$(_lastword)

    commands="status cond debug help kill ls log version  list enable	\
	      disable touch show cat edit create delete reload start	\
	      stop restart signal cgroup ps top plugins runlevel reboot	\
	      halt poweroff suspend utmp"
    cond_cmds="set get clear status dump"
    cond_types="hook net pid service task usr"
    signals="int term hup stop tstp cont usr1 usr2 pwr"
    options="-b --batch				\
	     -c --create			\
	     -d --debug				\
	     -f --force				\
	     -h --help				\
	     -j --json				\
	     -n --noerr				\
	     -1 --once				\
	     -p --plain				\
	     -q --quiet				\
	     -t --no-heading			\
	     -v --verbose			\
	     -V --version"

    case "${firstword}" in
	enable)
	    COMPREPLY=($(compgen -W "$(_available)" -- $cur))
	    ;;
	disable|touch)
	    COMPREPLY=($(compgen -W "$(_enabled)" -- $cur))
	    ;;
	show|cat|edit|delete)
	    COMPREPLY=($(compgen -W "$(_svc .)" -- $cur))
	    ;;
	start|stop|restart|log|status)
	    COMPREPLY=($(compgen -W "$(_ident)" -- $cur))
	    ;;
	signal|kill)
	    if $(_ident "${prev}"); then
		COMPREPLY=($(compgen -W "$signals" -- $cur))
	    else
		COMPREPLY=($(compgen -W "$(_ident)" -- $cur))
	    fi
	    ;;
	cond)
	    case "${lastword}" in
		set|clear)
		    compopt -o nospace
		    COMPREPLY=($(compgen -W "usr/" -- $cur))
		    ;;
		get)
		    COMPREPLY=($(compgen -W "$(_cond)" -- $cur))
		    ;;
		dump)
		    COMPREPLY=($(compgen -W "$cond_types" -- $cur))
		    ;;
		*)
		    COMPREPLY=($(compgen -W "$cond_cmds" -- $cur))
		    ;;
	    esac
	    ;;
	*)
	    COMPREPLY=($(compgen -W "$commands" -- $cur))
	    ;;
    esac

    if [[ $cur == -* ]]; then
	COMPREPLY=($(compgen -W "$options" -- $cur))
    fi
}

complete -F _initctl initctl
