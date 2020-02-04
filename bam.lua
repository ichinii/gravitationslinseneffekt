src_dir = "src"
obj_dir = "obj"
conf = ScriptArgs["conf"] or "debug"
build_dir = PathJoin("build", conf)
name = "bin_"..conf

AddTool(function(s)
	s.cc.flags:Add("-Wall")
	s.cc.flags:Add("-Wextra")
	s.cc.flags:Add("--std=c++2a")
	s.link.libs:Add("GL")
	s.link.libs:Add("GLU")
	s.link.libs:Add("GLEW")
	s.link.libs:Add("glfw")
	s.cc.includes:Add(src_dir)

	s.cc.Output = function(s, input)
		input = input:gsub("^"..src_dir.."/", "")
		return PathJoin(obj_dir, PathBase(input))
	end
	s.link.Output = function(s, input)
		return input
	end
end)

s = NewSettings()

src = CollectRecursive(PathJoin(src_dir, "*.cpp"))
obj = Compile(s, src)
bin = Link(s, name, obj)
PseudoTarget("compile", bin)
PseudoTarget("c", bin)
DefaultTarget("c");

AddJob("r", "running '"..bin.."'...", "./"..bin)
AddDependency("r", bin)
