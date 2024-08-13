$fn = 60;

rod_d = 8;

thickness = 2;
height = 8;

difference() {
    intersection() {
        translate([0,0,-10])
        sphere(r=20);
        translate([-20,-20,0])
            cube([40, 40, 20]);
    }
    translate([0,0,-1])
cylinder(r=rod_d/2, h = 16);
    translate([0,0,-.1])
    cylinder(r=15, h=2);
}
