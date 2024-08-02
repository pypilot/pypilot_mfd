$fn=120;
border = 8  ;
border_ex = 8;

enclosure = [90, 116.25];
board = [86.25, 116.25];
screen_off = 8;
screen = [60, 100];

conn = [19, 4.5];

connoff = 0;

thickness = 1.8;

height = 12;

gasket_d=3.5;

module ccube(x, y, z)
    translate([-x/2, -y/2, 0])
        cube([x, y, z]);

module keypad() {
    translate([12,0,0]) {
       translate([-8,0,0])
     cube([conn[1], conn[0], height]);
    minkowski() {
        cube([25-12, 80-12, 2],center=true);
        cylinder(r=6, h=1);
    }
}
}

oring_r = 15;
module oring_corner()
{
    translate([enclosure[0]/2-oring_r, enclosure[1]/2-oring_r, height])
        rotate_extrude(angle=90)
        translate([oring_r,0])
            circle(r=gasket_d/2);
}

module oring_side1()
{
        translate([0,enclosure[1]/2,height])
    rotate([0,90,0])
    cylinder(r=gasket_d/2, h=enclosure[0] - oring_r*2, center=true);
}

module oring_side2()
{
        translate([enclosure[0]/2,0,height])
    rotate([90,0,0])
    cylinder(r=gasket_d/2, h=enclosure[1] - oring_r*2, center=true);
}


module oring()
{
    oring_corner();
    mirror()
    oring_corner();
    mirror([0,1,0]) {
    oring_corner();
    mirror()
    oring_corner();
    }
    oring_side1();
    mirror([0,1,0])
    oring_side1();
    oring_side2();
    mirror()
    oring_side2();
}



module screw() {
    translate([0,0,28])
     cylinder(r=1.9, r2=4, h=3);
     cylinder(r=1.9, h=28);
     //cylinder(r=3.5, h=4, $fn=6);
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

module screws() {
    all() {
        translate([enclosure[0]*.12, enclosure[1]/2+border*.6, -.1])
            screw();
        translate([enclosure[0]*.36, enclosure[1]/2+border*.6, -.1])
            screw();
        translate([enclosure[0]/2+border*.6, 0,-.1])
            screw();
        translate([enclosure[0]/2+border*.6, enclosure[1]*.41,-.1])
            screw();
        translate([enclosure[0]/2+border*.6, enclosure[1]*.21, -.1])
            screw();
    }
}


module bottom() {
difference() {
    minkowski() {
        ccube(enclosure[0]-border_ex*2, enclosure[1]-border_ex*2, height);
            cylinder(r=border+border_ex,h=.1);
    }
    translate([0,0,thickness])
        ccube(enclosure[0], enclosure[1], height-6);
    translate([0,0,thickness])
    minkowski() {
        ccube(enclosure[0]-oring_r-12, enclosure[1]-oring_r-12, height-thickness-2.3);
            cylinder(r=oring_r-2,h=.1);
    }
    
    hull()
    oring();

    translate([screen_off,0,-thickness])
        ccube(screen[0], screen[1], 5);
    
    translate([-board[0]/2-4, connoff, -1]) {
        keypad();
    }
    
    screws();
}
}


//bottom();


//module screw()
  //  cylinder(r=1.8, h=10);

ht=6;
module oring_box(inner)
{
    difference() {
        cylinder(r=80, h=ht);
        translate([0,0,ht-.5])
        rotate_extrude() {
            translate([70,0])
                circle(r=gasket_d/2);
        }
        translate([0,0,2])
            if(inner)
                cylinder(r=60, h=ht); 
            else                    
                cylinder(r=66, h=ht); 
        for(i=[0:10])
            rotate(i*36)
                translate([75,0,0])
                    screw();
        if(inner)
            translate([0,0,ht-2.6])
            difference() {
                cylinder(r=82, h=ht);
                cylinder(r=70, h = ht+1);
            }
    }
}
oring_box(1);
