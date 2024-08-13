 $fn=60;

rod_diameter=8;
rod_offset=2.5;
rod_z=0;

width = 54.2;
wid_marg = 8;
height = 30;
ht_marg = 2;
depth = 6.4;

module board() {
    cube([width, height+2, 2.5], center=true);
}

module all() {
    children();
    mirror([1, 0, 0])
    children();
    mirror([0, 1, 0])
    children();
    mirror([1, 0, 0])
    mirror([0, 1, 0])
    children();
}

extra_height=8;

module box() {
    difference() {
        translate([0,extra_height/2,0])
        minkowski() {
            cube([width+2*wid_marg, height+2*ht_marg+extra_height, depth],center=true);
            sphere(r=2);
        }
    
        translate([0,extra_height/2,0])
        all()
            translate([width*.43+wid_marg, (height+extra_height)*.35+ht_marg, 0])
                cylinder(r=2, h=height,center=true);
    
        translate([0,-4,0])
            cube([width-2,height,depth],center=true);
     }
}

support_len=30;
module support() {
    hull() {
        translate([-rod_offset,height/2+2+support_len,rod_z])
            rotate([90,0,0])
                cylinder(r=rod_diameter/2+1.2, h=support_len);
    
        translate([-rod_offset,height/2+1,rod_z])
            rotate([0,90,0])
                cylinder(r=5.2, h=width+1, center=true);
    }
}


module holder() {
difference() {
   union() {
        box();
        support();
    }
    translate([0,-3,-.8])
        board();
    
    translate([-rod_offset,height/2+support_len+3.8,rod_z])
        rotate([90,0,0])
            cylinder(r=rod_diameter/2+.3, h=support_len+10);
}
}

if(0) {
rotate([180,0,0])
intersection() {
    translate([-100, -100,0])
        cube([200,200,200]);
    holder();
}
}
else {
intersection() {
    translate([-100, -100,-200])
        cube([200,200,200]);
    holder();
}

}