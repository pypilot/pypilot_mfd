$fn=30;

use_threads = false;

length = 124;
width = 96;
height = 20;

board_length = 116.25;
board_width = 86.25-4;

pane = [128,100, 3];

thickness = 8;
bthickness = 1.8;

conn = [19, 3.5];

connoff = -19;

lcd_area = [101, 60];
lcd_off = 3;

board_off=-0;
module box() {
    difference() {
        translate([-length/2,-width/2,0])
        minkowski() {
            cube([length, width, height]);
            cylinder(r=thickness,h=bthickness);
        }
        translate([-board_length/2,-board_width/2-board_off,bthickness])
        cube([board_length, board_width, height+bthickness]);
        translate([-board_length/2,-board_width/2-board_off-4,bthickness])
        cube([board_length, board_width, height-bthickness]);
    
        // usb port
         translate([10,-.8-board_off,0])
        cylinder(r=6.5, h=40, center=true);
        
        // rs422 port
     //    translate([-8,-.8-board_off,0])
       // cylinder(r=6.5, h=40, center=true);

    translate([0,0,height+bthickness])
        scale([1, 1, .7])
        groove();

        //translate([-pane[0]/2,-pane[1]/2,height+bthickness-pane[2]+.1])
      //     cube(pane);
        translate([0,0,-1])
        screws();
        
        translate([connoff,pane[1]/2-9,3])
          cube([conn[0], conn[1], height]);
        //translate([connoff,pane[1]/2-8,3])
          //cube([conn[0], 15, height-pane[2]-5   ]);
        
        translate([-board_length/2-thickness-5, 0, height/2])
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
            cylinder(r=gasket_d/2, h=width-thickness*2, center=true);
    translate([0, width/2,0])
        rotate([0,90,0])
            cylinder(r=gasket_d/2, h=length-thickness*2, center=true);
}

module corner_groove() {
    translate([length/2-thickness, width/2-thickness,0])
        rotate_extrude(angle=90)
    translate([thickness,0])
            circle(r=gasket_d/2);
}

module screw() {
     cylinder(r=1.6,h=30);
     cylinder(r=3,h=4,$fn=6);
}

module screws() {
all() {
    translate([length/2+thickness*.55, 0, -.1])
        screw();
    translate([length/2+thickness*.55, width*.35, -.1])
        screw();
    translate([length*.42,width/2+thickness*.55, -.1])
        screw();
    translate([length*.16, width/2+thickness*.55, -.1])
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
        cylinder(r=3, h=5);
    
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
        screws();
    }
}



//nut(); 

//translate([0,0,20])
if(1)
enclosure();
if(0)
//translate([0,width + thickness*3, 0])

translate([0,0, height+3])

frame();
