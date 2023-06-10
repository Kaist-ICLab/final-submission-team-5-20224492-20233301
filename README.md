# howManyShotsAreYou
2023 Spring KAIST CS565 Mini project <br>
“How many shots are you?”: A Just-in-Time Intervention System to Prevent Overdrinking based on Soju-Glassware Motion Detection  <br>

- [Subin Park](https://github.com/twinklesu) 
- [Injae Song](https://github.com/neverdiesong)


## Folder structure
```
.
├── data_collection                   # Code for collecting data to train model 
│   └── data_collection.ino
├── dataset                           # Dataset used to train model
│   ├── half_shot.csv         
│   ├── half_shot_2.csv
│   ├── one_shot.csv     
│   └── one_shot_2.csv            
├── in_lab_shot_detection             # In lab test code for "How many shots are you" 
│   ├── in_lab_shot_detection.ino          
│   └── model.h            
├── stand_alone_shot_detection        # Final code for stand alone "How many shots are you" 
│   ├── stand_alone_shot_detection.ino          
│   └── model.h            
└── model_build_colab.ipynb           # Colab code to build model
```

## 2 versions 
1. In-lab shot detection
  - To run the system in in-lab condition, execute `in_lab_shot_detection.ino` with wire connection to PC. It will shows you the limit number, the results of motion detection, and drinking status on your 'Serial monitor'.
2. Stand alone shot detection
 - To run the system wirelessly, execute `stand_alone_shot_detection.ino`. Follow the instruction to connect LED. 
  1. Connect traffic light LED to Arduino.
    <img width="1439" alt="image" src="https://github.com/twinklesu/howManyShotsAreYou/assets/68603692/e83f2cfa-a089-4707-8848-07b6edd61e80">
  2. Attach Arduino on soju-glassware as the following picture.
    <img width="1411" alt="image" src="https://github.com/twinklesu/howManyShotsAreYou/assets/68603692/596c881e-a9ec-409c-b58d-76f7198ecce8">
  3. Connect the Arduino with battery.
