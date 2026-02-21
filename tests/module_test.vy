module vcore;
module vmem;
module CustomModule;

fn::CustomModule call(){
    out("hi");
}

out(vmem.usage(CustomModule.call));