package main

import (
	"flag"
	"fmt"
	"github.com/golang/protobuf/proto"
	"github.com/pkg/errors"
	"google.golang.org/protobuf/reflect/protodesc"
	"google.golang.org/protobuf/reflect/protoreflect"
	"google.golang.org/protobuf/reflect/protoregistry"
	"google.golang.org/protobuf/types/descriptorpb"
	"io/ioutil"
	"os"
	"os/exec"
	"path"
)

func registerProtoFile(srcDir string, filename string) error {
	// First, convert the .proto file to a file descriptor set.
	tmpFile := path.Join(srcDir, filename+".pb")
	cmd := exec.Command("protoc",
		"--include_source_info",
		"--descriptor_set_out="+tmpFile,
		"--proto_path="+srcDir,
		path.Join(srcDir, filename))

	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	err := cmd.Run()
	if err != nil {
		return errors.Wrapf(err, "protoc")
	}

	defer os.Remove(tmpFile)

	// Now load that temporary file as a file descriptor set protobuf.
	protoFile, err := ioutil.ReadFile(tmpFile)
	if err != nil {
		return errors.Wrapf(err, "read tmp file")
	}

	pbSet := new(descriptorpb.FileDescriptorSet)
	if err := proto.Unmarshal(protoFile, pbSet); err != nil {
		return errors.Wrapf(err, "unmarshal")
	}

	// We know protoc was invoked with a single .proto file.
	pb := pbSet.GetFile()[0]

	// Initialize the file descriptor object.
	fd, err := protodesc.NewFile(pb, protoregistry.GlobalFiles)
	if err != nil {
		return errors.Wrapf(err, "NewFile")
	}

	// and finally register it.
	return protoregistry.GlobalFiles.RegisterFile(fd)
}

// ╔═════════╤═════════════════════════════════════╗
// ║ Go type │ Protobuf kind                       ║
// ╠═════════╪═════════════════════════════════════╣
// ║ bool    │ BoolKind                            ║
// ║ int32   │ Int32Kind, Sint32Kind, Sfixed32Kind ║
// ║ int64   │ Int64Kind, Sint64Kind, Sfixed64Kind ║
// ║ uint32  │ Uint32Kind, Fixed32Kind             ║
// ║ uint64  │ Uint64Kind, Fixed64Kind             ║
// ║ string  │ StringKind                          ║
// ╚═════════╧═════════════════════════════════════╝
func field_kind_name(field protoreflect.FieldDescriptor) string {
	kind := field.Kind()
	switch kind {
	case protoreflect.BoolKind:
		return "bool"
	case protoreflect.Int32Kind:
		return "int32"
	case protoreflect.Sint32Kind:
		return "int32"
	case protoreflect.Sfixed32Kind:
		return "int32"
	case protoreflect.Int64Kind:
		return "int64"
	case protoreflect.Sint64Kind:
		return "int64"
	case protoreflect.Sfixed64Kind:
		return "int64"
	case protoreflect.Uint32Kind:
		return "uint32"
	case protoreflect.Fixed32Kind:
		return "uint32"
	case protoreflect.Uint64Kind:
		return "uint64"
	case protoreflect.Fixed64Kind:
		return "uint64"
	case protoreflect.FloatKind:
		return "float"
	case protoreflect.DoubleKind:
		return "double"
	case protoreflect.StringKind:
		return "string"
	case protoreflect.BytesKind:
		return "string"
	case protoreflect.MessageKind:
		return string(field.Message().Name())
	case protoreflect.EnumKind:
		return "int32"
	}
	panic("unknown kind " + kind.String())
}

func filed_kind_size(field protoreflect.FieldDescriptor) int {
	ctype := field_kind_name(field)
	switch ctype {
	case "bool":
		return 2
	case "int32":
		return 5
	case "int64":
		return 9
	case "uint32":
		return 5
	case "uint64":
		return 9
	case "float":
		return 5
	case "double":
		return 9
	case "string":
		return 9
	default:
		return 9 // void*
	}
}

func is_shared_obj(field protoreflect.FieldDescriptor) int {
	kind := field.Kind()
	switch kind {
	case protoreflect.BoolKind:
		return 0
	case protoreflect.Int32Kind:
		return 0
	case protoreflect.Sint32Kind:
		return 0
	case protoreflect.Sfixed32Kind:
		return 0
	case protoreflect.Int64Kind:
		return 0
	case protoreflect.Sint64Kind:
		return 0
	case protoreflect.Sfixed64Kind:
		return 0
	case protoreflect.Uint32Kind:
		return 0
	case protoreflect.Fixed32Kind:
		return 0
	case protoreflect.Uint64Kind:
		return 0
	case protoreflect.Fixed64Kind:
		return 0
	case protoreflect.FloatKind:
		return 0
	case protoreflect.DoubleKind:
		return 0
	case protoreflect.StringKind:
		return 1
	case protoreflect.BytesKind:
		return 1
	case protoreflect.MessageKind:
		return 1
	case protoreflect.EnumKind:
		return 0
	}
	panic("unknown kind " + kind.String())
}

func main() {
	input_dir := flag.String("d", "./", "input dir")
	input := flag.String("i", "input.proto", "input file")
	output := flag.String("o", "output.lua", "output file")

	flag.Parse()

	input_file := *input
	output_file := *output

	err := registerProtoFile(*input_dir, input_file)
	if err != nil {
		panic(errors.Wrapf(err, input_file))
	}

	var ret []protoreflect.FileDescriptor
	protoregistry.GlobalFiles.RangeFiles(func(descriptor protoreflect.FileDescriptor) bool {
		ret = append(ret, descriptor)
		return true
	})

	var all_messages []protoreflect.MessageDescriptor
	for _, f := range ret {
		if f.FullName() == "cpp_table" {
			for i := 0; i < f.Messages().Len(); i++ {
				m := f.Messages().Get(i)
				all_messages = append(all_messages, m)
			}
		}
	}

	output_str := "-- Generated by the tools.  DO NOT EDIT!\n" +
		"_G.CPP_TABLE_PROTO = _G.CPP_TABLE_PROTO or {}\n"
	for _, m := range all_messages {
		output_str += fmt.Sprintf("_G.CPP_TABLE_PROTO.%s = {\n", m.Name())
		for j := 0; j < m.Fields().Len(); j++ {
			field := m.Fields().Get(j)
			// name = { type = "normal", key = "string", tag = 1 },
			// check is repeated
			if field.Cardinality() == protoreflect.Repeated {
				// check is map<>
				if field.IsMap() {
					output_str += fmt.Sprintf("    %s = { type = \"map\", key = \"%s\", value = \"%s\", tag = %d, size = %d, shared = 1 },\n", field.Name(), field_kind_name(field.MapKey()), field_kind_name(field.MapValue()), field.Number(), 9)
				} else {
					output_str += fmt.Sprintf("    %s = { type = \"array\", key = \"%s\", tag = %d, size = %d, shared = 1, key_size = %d, key_shared = %d },\n", field.Name(), field_kind_name(field), field.Number(), 9, filed_kind_size(field), is_shared_obj(field))
				}
			} else {
				output_str += fmt.Sprintf("    %s = { type = \"normal\", key = \"%s\", tag = %d, size = %d, shared = %d },\n", field.Name(), field_kind_name(field), field.Number(), filed_kind_size(field), is_shared_obj(field))
			}
		}
		output_str += "}\n"
	}

	fmt.Println(output_str)

	out, err := os.Create(output_file)
	if err != nil {
		fmt.Println("create file fail ", err)
		return
	}
	defer out.Close()
	out.WriteString(output_str)
}
