$fn=40;

length = 116;
width = 85;
height = 15;

pane = [118,88, 3];

thickness = 8;
bthickness = 2;

module box() {
    difference() {
        translate([-length/2,-width/2,0])
        minkowski() {
            cube([length, width, height]);
            cylinder(r=thickness,h=bthickness);
        }
        translate([-length/2,-width/2,bthickness])
        cube([length, width, height+bthickness]);
    
        cylinder(r=7, h=40, center=true);

        translate([0,0,height+bthickness])
            cube(pane, center=true);
        translate([0,0,height])
        screws();
    }   

}

module screw()
     cylinder(r=1.3,h=10);


module screws() {
all() {
    translate([length/2+thickness*.5, 0, -.1])
        screw();
    translate([length/2+thickness*.4, width/2+thickness*.4, -.1])
        screw();
    translate([length/6+thickness*.5, width/2+thickness*.5, -.1])
        screw();
}
}

module all() {
    children();
    mirror([1,0,0])
    children();
    mirror([0,1,0]) {
        children();
        mirror([1,0,0])
        children();
    }
}

module enclosure() {
    box();
    all()
        translate([length*.3, width*.3])
        cylinder(r=3, h=3);
}

module frame() {
    difference() {
     translate([0,0,pane[2]/2])
        minkowski() {
            cube([length, width, pane[2]], center=true);
            cylinder(r=thickness,h=1);
        }
         cube([length, width, height], center=true);
        screws();
    }
}
enclosure();

translate([0,width + thickness*3, 0])
frame();
