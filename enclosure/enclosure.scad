$fn=40;

use_threads = false;

length = 120;
width = 94;
height = 17;

board_length = 116.25;
board_width = 86.25;

pane = [124,94, 4];

thickness = 8;
bthickness = .9;

conn = [19, 3.5];

connoff = -18;

lcd_area = [111, 64];
lcd_off = 5;


module box() {
    difference() {
        translate([-length/2,-width/2+2,0])
        minkowski() {
            cube([length, width, height]);
            cylinder(r=thickness,h=bthickness);
        }
        translate([-board_length/2,-board_width/2,bthickness])
        cube([board_length, board_width, height+bthickness]);
    
         translate([10,-.8,0])
        cylinder(r=6.5, h=40, center=true);
         translate([-8,-.8,0])
        cylinder(r=6.5, h=40, center=true);

        translate([0,0,height+bthickness])
            cube(pane, center=true);
       // translate([0,0,height])
       // screws();
        
        translate([connoff,pane[1]/2+1,3])
          cube([conn[0], conn[1], height]);
        translate([connoff,pane[1]/2-8,3])
          cube([conn[0], 10, height-pane[2]-5   ]);
        
        translate([-board_length/2-thickness-1, 0, height/2])
        rotate([0, -90, 0])
        rotate(-90)
            linear_extrude(height = 2)
                text("pypilot", size = 10, halign = "center", valign = "center", $fn = 16);  
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

use <threads-library-by-cuiso-v1.scad>

module enclosure() {
    box();
    all()
        translate([length*.3, width*.3])
        cylinder(r=3, h=1.4);
    
    if(use_threads)
    translate([1,0,-20])
        difference() {
            thread_for_screw(diameter=48, length=20);
           translate([0,0,-1]) 
            cylinder(r=18, h=22);
        }
}

module nut() {
difference(){
    union() {
cylinder(r=35, h=2);
   translate([0,0,2]) 
cylinder(r1=35, r2=29, h=2);
   translate([0,0,4]) 
cylinder(r=30, h=6, $fn = 10);
    } 
    translate([0,0,-.1])
    thread_for_nut(diameter=48, length=20.2, usrclearance=0.3); 
}
}

module keypad() {
    minkowski() {
        cube([80-12, 25-12, 2],center=true);
        cylinder(r=6, h=5);
    }
}

module frame() {
    difference() {
     translate([0,2,0])
        minkowski() {
            cube([length, width, 1], center=true);
            cylinder(r=thickness,h=1);
        }
        translate([0,-lcd_off,0])
        minkowski() {
         cube([lcd_area[0]-6, lcd_area[1]-6, height], center=true);
            cylinder(r=3);
        }
        translate([0, pane[1]/2-5, 0])
        keypad();
       ;// screws();
    }
}



//nut();

//translate([0,0,20])
if(0)
enclosure();
if(1)
//translate([0,width + thickness*3, 0])

translate([0,0, height+3])

frame();
