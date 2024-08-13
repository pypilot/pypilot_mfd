$fn = 60;

rod_d = 8;

thickness = 2;
height = 5;

difference() {
    union() {
    cylinder(r=rod_d/2+thickness, h = height);
    cylinder(r=9, h = height-1);
    cylinder(r=14.5, h = height-4);
    }
    translate([0,0,-1])
cylinder(r=rod_d/2, h = height+2);
}
