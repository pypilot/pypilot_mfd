$fn=60;
border = 8  ;
border_ex = 8;

enclosure = [90, 107];
board = [75, 90];
screen_off = 7;
screen = [76, 106];
screen_area = [56, 98]; // visible area

conn = [19, 4.5];

connoff = 0;
keypadoff = 42;

thickness = 1.8;

height = 12;
theight = 8;

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
    translate([enclosure[0]/2-oring_r, enclosure[1]/2-oring_r, 0])
        rotate_extrude(angle=90)
        translate([oring_r,0])
            circle(r=gasket_d/2);
}

module oring_side1()
{
        translate([0,enclosure[1]/2,0])
    rotate([0,90,0])
    cylinder(r=gasket_d/2, h=enclosure[0] - oring_r*2, center=true);
}

module oring_side2()
{
        translate([enclosure[0]/2,0,0])
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



module screw(r, head) {
    if(head)
     cylinder(r2=r, r1=4, h=3);
     cylinder(r=r, h=28);
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

module screws(r) {
    all() {
        translate([enclosure[0]*.14, enclosure[1]/2+border*.6, -.1])
            screw(r);
        translate([enclosure[0]*.39, enclosure[1]/2+border*.6, -.1])
            screw(r);
        translate([enclosure[0]/2+border*.6, 0,-.1])
            screw(r);
        translate([enclosure[0]/2+border*.6, enclosure[1]*.42,-.1])
            screw(r);
        translate([enclosure[0]/2+border*.6, enclosure[1]*.21, -.1])
            screw(r);
    }
}

module main_box(ht)
{
        minkowski() {
        ccube(enclosure[0]-border_ex*2, enclosure[1]-    border_ex*2, ht);
            cylinder(r=border+border_ex,h=.1);
    }
}

module inner_box(ht)
{
        translate([0,0,thickness])
    minkowski() {
        ccube(enclosure[0]-oring_r-12, enclosure[1]-oring_r-12, ht-thickness-2.3+5);
            cylinder(r=oring_r-2,h=.1);
    }
}

module top() {
  difference() {
    // main box with rounded corners
    union() {
        main_box(theight);
        inner_box(theight);
    }
    translate([0,0,theight+2])
      oring();
    
    screws(1.9, true);
    
     translate([0,0,5])
    ccube(board[0], board[1], 30);
  }
}

module bottom() {
  difference() {
      
    main_box(height);
    inner_box(height);
    
    translate([0,0,height-.5])
        hull()
            oring();

    translate([screen_off,0,thickness-.3])
        ccube(screen[0], screen[1], 6.2);
    translate([screen_off+7,0,-thickness])
        ccube(screen_area[0], screen_area[1], 5);
    
     translate([-24, 40, -1])
      cylinder(r=2,h=thickness+2);
      
    translate([-keypadoff, connoff, -1]) {
        keypad();
    }

    translate([0,0,2])    
    screws(1.4,false);
  }
  translate([0,0,12])
  #  ccube(board[0], board[1], 3);
}

translate([120,0,0])top();

bottom();

//module screw()
  //  cylinder(r=1.8, h=10);
