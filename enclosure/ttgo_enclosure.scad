$fn=60;

length=51.5;
width=25.7;
height=5.5;

ft=2;
fh = 1.5;

thickness = 1.4;

module usb_c_hole() {
    rotate([0,90,0])
    hull() {
        translate([0, -3, 0])
            cylinder(r=1.9, h=10);
        translate([0, 2.9, 0])
            cylinder(r=1.9, h=10);
    }
}

difference() {
   
        translate([thickness, thickness, thickness])
        minkowski() {
               cube([length+ft*2, width+ft*2, height+fh]);
                sphere(thickness);
        }
    
    translate([thickness+ft, thickness+ft, thickness])
    cube([length, width, height+5]);
        
    translate([thickness, thickness, height+thickness])
        cube([length+ft*2, width+ft*2, 10]);
        
            translate([thickness+11, thickness+.5, thickness])
        cube([5, 10, 10]);

    
    translate([0, width/2+thickness+ft, thickness+height/2+1])
    usb_c_hole();
        
        translate([20,.3,height/2])
rotate([90,0,0])
linear_extrude(2)
text("pypilot",  size=5);

}

