
INT_FLAGS += -DMG_ARCH=MG_ARCH_CUSTOM

mgo:
	(cd pkgs/mongoose; rm -f mongoose.cpp mongoose.h)
	(cd pkgs/mongoose; ln -s mongoose/mongoose_5.6.h mongoose.h)
	(cd pkgs/mongoose; ln -s mongoose/mongoose_5.6.cpp mongoose.cpp)
	(cd web; rm -f web.h web.cpp web_util.cpp web_server.cpp)
	(cd web; ln -s web/web_5.6.h web.h)
	(cd web; ln -s web/web_5.6.cpp web.cpp)
	(cd web; ln -s web/web_util_5.6.cpp web_util.cpp)
	(cd web; ln -s web/web_server_5.6.cpp web_server.cpp)

mgn:
	(cd pkgs/mongoose; rm -f mongoose.cpp mongoose.h)
	(cd pkgs/mongoose; ln -s mongoose/mongoose_7.14.h mongoose.h)
	(cd pkgs/mongoose; ln -s mongoose/mongoose_7.14.cpp mongoose.cpp)
	(cd web; rm -f web.h web.cpp web_util.cpp web_server.cpp)
	(cd web; ln -s web/web_7.14.h web.h)
	(cd web; ln -s web/web_7.14.cpp web.cpp)
	(cd web; ln -s web/web_util_7.14.cpp web_util.cpp)
	(cd web; ln -s web/web_server_7.14.cpp web_server.cpp)

mgs:
	@(cd pkgs/mongoose; ls -la mongoose.cpp mongoose.h)
	@(cd web; ls -la web.h web.cpp web_util.cpp web_server.cpp)

mgtouch:
	(cd pkgs/mongoose; touch mongoose/*)

mgd5:
	colordiff -br ../proj/mongoose/diff-5.6 pkgs/mongoose/mongoose

mgd7:
	colordiff -br ../proj/mongoose/diff-7.14 pkgs/mongoose/mongoose
