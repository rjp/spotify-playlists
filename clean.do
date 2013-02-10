if [ -e .do_built ]; then
	while read x; do
		[ -d "$x" ] || rm -f "$x"
	done <.do_built
fi
[ -z "$DO_BUILT" ] && rm -rf .do_built .do_built.dir
