$fn=120;

use_threads = true;
use_holes = false;

length = 124;
width = 97;
height = 19;

board_length = 116.25;
board_width = 86.25-4;

pane = [128,100, 3];

thickness = 8;
bthickness = 1.8;

conn = [19, 4.5];

connoff = -24;

lcd_area = [101, 60];
lcd_off = 3;

ex_r = 3;

board_off=4;
module box() {
    difference() {
        translate([-length/2+ex_r,-width/2+ex_r+1,0])
        minkowski() {
            cube([length-ex_r*2, width-ex_r*2, height]);
            cylinder(r=thickness+ex_r,h=bthickness);
        }
        translate([-board_length/2,-board_width/2,bthickness])
        cube([board_length, board_width, height+bthickness]);
        translate([-board_length/2,-board_width/2-board_off,bthickness])
        cube([board_length, board_width, height-bthickness-1]);
    
        // usb port
        if(1) {
        translate([5,-5-board_off,0])
            cylinder(r=6.5, h=40, center=true);
        
        // rs422 port
        translate([5,13-board_off,0])
            cylinder(r=6.5, h=40, center=true);
        }
        translate([0,1,height+bthickness-.8])
            scale([1, 1, .7])
                groove();

        //translate([-pane[0]/2,-pane[1]/2,height+bthickness-pane[2]+.1])
      //     cube(pane);
        translate([0,+1,-1])
        screws();
        
        if(use_holes)
            holes();
        // keypad ribbon
        translate([connoff,pane[1]/2-9,4])
          cube([conn[0], conn[1], height]);
        // wifi antenna area
        translate([10,pane[1]/2-9,5])
          cube([45, 8, height-pane[2]-3   ]);
        
        translate([-board_length/2-thickness-3, 0, height/2])
        rotate([0, -90, 0])
        rotate(-90)
            linear_extrude(height = 2)
                text("pypilot", size = 10, halign = "center", valign = "center", $fn = 16);  
    }
}


gasket_d = 4;
groove_off = 4;
module groove() {
    side_groove();
    mirror([1,0,0])
    mirror([0,1,0])
    side_groove();
all()
    corner_groove();
}

module side_groove() {
        translate([length/2,0,0])
        rotate([90,0,0])
            cylinder(r=gasket_d/2, h=width-thickness*2-ex_r*2+.1, center=true);
    translate([0, width/2,0])
        rotate([0,90,0])
            cylinder(r=gasket_d/2, h=length-thickness*2-ex_r*2+.1, center=true);
}

module corner_groove() {
    translate([length/2-thickness-ex_r, width/2-thickness-ex_r,0])
        rotate_extrude(angle=90)
    translate([thickness+ex_r,0])
            circle(r=gasket_d/2);
}

module holea()
{
        translate([0,0,height/2])
            rotate([90,0,0])
                cylinder(r=height/2-2, h=width+1, center=true);
}

module holeb()
{
        translate([0,0,height/2])
            rotate([0,90,0])
                cylinder(r=height/2-2, h=length+1, center=true);
}

module holes()
{
    translate([32,0,0])
        holea();
    translate([-32,0,0])
        holea();
        holea();
    translate([0,16,0])
        holeb();
    translate([0,-16,0])
        holeb();
}

module screw(r) {
    translate([0,0,28])
     cylinder(r1=r, r2=4, h=3);
     cylinder(r=r, h=28);
     cylinder(r=3.5, h=4, $fn=6);
}

module screws(r=1.6) {
    all() {
        translate([length/2+thickness*.55, 0, -.1])
            screw(r);
        translate([length/2+thickness*.55, width*.35, -.1])
            screw(r);
        translate([length*.4,width/2+thickness*.55, -.1])
            screw(r);
        translate([length*.15, width/2+thickness*.55, -.1])
            screw(r);
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

sr = 1.4;
module support()
{
       hull() {
        translate([length*.2, width/2])
            cylinder(r=sr, h=5);
        translate([-length*.6, -width/2])
            cylinder(r=sr, h=4);
   }
       hull() {
        translate([length*.7, width/2])
            cylinder(r=sr, h=5);
        translate([-length*.1, -width/2])
            cylinder(r=sr, h=5);
   }
       hull() {
        translate([-length*.6, width/2])
            cylinder(r=sr, h=5);
        translate([length*.1, -width/2])
            cylinder(r=sr, h=5);
   }

       hull() {
        translate([-length*.1, width/2])
            cylinder(r=sr, h=5);
        translate([length*.6, -width/2])
            cylinder(r=sr, h=5);
   }
}

thread_len=36;
module enclosure() {
    box();
    all()
        translate([length*.3, width*.3])
        cylinder(r=5, h=6);
    intersection() {
    support();
        
        cube([length, width, height],center=true);
    }
   
    if(use_threads)
    translate([5,0,-thread_len])
        difference() {
            union() {
                translate([0,0,thread_len-4.5])
                cylinder(r1=21, r2=24, h=5);
                translate([0,0,thread_len-11])
                cylinder(r=21.5, h=11);
            thread_for_screw(diameter=48, length=thread_len-10);
            }
           translate([0,0,-1]) 
            cylinder(r=18, h=thread_len+2);
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
     translate([0,0,0])
        minkowski() {
            cube([length, width, 1], center=true);
            cylinder(r=thickness,h=1);
        }
        translate([0,-lcd_off,0])
        minkowski() {
         cube([lcd_area[0]-6, lcd_area[1]-6, height], center=true);
            cylinder(r=3);
        }
        translate([0, pane[1]/2-10, 0])
        keypad();
        translate([0,0,-29])
       screws();
    }
}


module cover() {
    difference() {
     translate([0,0,0])
        minkowski() {
            cube([length, width, 1], center=true);
            cylinder(r=thickness,h=1);
        }

                
        translate([connoff,pane[1]/2-9,-2])
          cube([conn[0], conn[1], height]);
        translate([0,0,-20])
       screws(2.2);
    }
}




//nut(); 

//translate([0,0,20])
if(1)
enclosure();
if(0)
//translate([0,width + thickness*3, 0])
projection()
translate([0,1, height+3])
cover();
//frame();
