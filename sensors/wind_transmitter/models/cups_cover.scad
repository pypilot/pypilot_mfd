$fn = 240;


key_r=.9;

rod_d = 8.1;

thickness = 2;
height = 8;

difference() {
    intersection() {
        translate([0,0,-9])
        sphere(r=20);
        translate([-20,-20,0])
            cube([40, 40, 20]);
    }
    translate([0,0,-1])
        cylinder(r=rod_d/2, h = 16);
    translate([0,0,-.1])
        cylinder(r=15, h=3);
    
   translate([0,0,6])
       rotate([0,90,0])
          cylinder(r=key_r, h=30);
    
   translate([0,0,-1])
        difference() {
            cylinder(r=20, h=31);
            cylinder(r=17, h=30);
        }
}
