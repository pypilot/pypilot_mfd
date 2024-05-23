$fn=40;

length = 116;
width = 86;
height = 19;

board_length = 116;

pane = [120,90, 2.2];

thickness = 7;
bthickness = .9;

module box() {
    difference() {
        translate([-length/2,-width/2,0])
        minkowski() {
            cube([length, width, height]);
            cylinder(r=thickness,h=bthickness);
        }
        translate([-board_length/2,-width/2,bthickness])
        cube([board_length, width, height+bthickness]);
    
         translate([10,-.8,0])
        cylinder(r=6.5, h=40, center=true);
         translate([-8,-.8,0])
        cylinder(r=6.5, h=40, center=true);

        translate([0,0,height+bthickness])
            cube(pane, center=true);
        translate([0,0,height])
        screws();
        
        translate([-board_length/2-thickness+1, 0, height/2])
        rotate([0, -90, 0])
        rotate(-90)
            linear_extrude(height = 2)
                text("pypilot", size = 10, font=font, halign = "center", valign = "center", $fn = 16);  
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
       ;// cylinder(r=3, h=3);
}

module frame() {
    difference() {
     translate([0,0,pane[2]/2])
        minkowski() {
            cube([length, width, 1], center=true);
            cylinder(r=thickness,h=1);
        }
         cube([length, width, height], center=true);
        screws();
    }
}
if(1)
enclosure();
else
translate([0,width + thickness*3, 0])
frame();
