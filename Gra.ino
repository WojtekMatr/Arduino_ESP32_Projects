#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define BUTTON_PIN 27
#define LED_PIN 26

#define PLAYER_X 10
#define PLAYER_SIZE 6
#define OBSTACLE_WIDTH 8

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Preferences prefs;

enum GameState {
  WAIT_START,
  PLAYING,
  GAME_OVER
};

GameState gameState = WAIT_START;

int lanesY[3] = {16, 34, 52};
int playerLane = 1;
int obstacleLane = 0;

int obstacleX = 128;
int score = 0;
int bestScore = 0;
int level = 1;
int speed = 2;

unsigned long lastFrame = 0;
bool lastButton = HIGH;

void resetGame() {
  playerLane = 1;
  obstacleX = 128;
  obstacleLane = random(0, 3);
  score = 0;
  level = 1;
  speed = 2;
  digitalWrite(LED_PIN, LOW);
}

void saveBestScore() {
  if (score > bestScore) {
    bestScore = score;
    prefs.putInt("best", bestScore);
  }
}

void setup() {
  Serial.begin(115200);

  Wire.begin(14, 13);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for (;;);
  }

  randomSeed(millis());

  prefs.begin("game", false);
  bestScore = prefs.getInt("best", 0);
}

void loop() {
  if (millis() - lastFrame < 40) return;
  lastFrame = millis();

  bool button = digitalRead(BUTTON_PIN);
  bool buttonClick = (lastButton == HIGH && button == LOW);
  lastButton = button;

  display.clearDisplay();

  if (gameState == WAIT_START) {
    display.setTextSize(1);
    display.setCursor(20, 18);
    display.println("GRA: 3 TORY");

    display.setCursor(18, 34);
    display.print("BEST: ");
    display.print(bestScore);

    display.setCursor(10, 52);
    display.println("Klik = start");
    display.display();

    if (buttonClick) {
      resetGame();
      gameState = PLAYING;
    }
    return;
  }

  if (gameState == GAME_OVER) {
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(2);
    display.setCursor(10, 16);
    display.println("KONIEC");

    display.setTextSize(1);
    display.setCursor(10, 40);
    display.print("Score: ");
    display.print(score);

    display.setCursor(10, 52);
    display.print("Best: ");
    display.print(bestScore);

    display.display();
    digitalWrite(LED_PIN, HIGH);

    if (buttonClick) {
      gameState = WAIT_START;
      digitalWrite(LED_PIN, LOW);
    }
    return;
  }

  if (gameState == PLAYING) {
    if (buttonClick) {
      playerLane = (playerLane + 1) % 3;
    }

    obstacleX -= speed;

    if (obstacleX < -OBSTACLE_WIDTH) {
      obstacleX = 128;
      obstacleLane = random(0, 3);
      score++;

      saveBestScore();

      if (score % 5 == 0) {
        level++;
        speed++;
      }
    }

    if (obstacleLane == playerLane &&
        obstacleX < PLAYER_X + PLAYER_SIZE &&
        obstacleX + OBSTACLE_WIDTH > PLAYER_X) {
      saveBestScore();
      gameState = GAME_OVER;
    }

    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.print("S:");
    display.print(score);
    display.print(" B:");
    display.print(bestScore);

    for (int i = 0; i < 3; i++) {
      display.drawLine(0, lanesY[i] + PLAYER_SIZE + 2, 128,
                       lanesY[i] + PLAYER_SIZE + 2, WHITE);
    }

    display.fillRect(PLAYER_X, lanesY[playerLane],
                     PLAYER_SIZE, PLAYER_SIZE, WHITE);

    display.fillRect(obstacleX, lanesY[obstacleLane],
                     OBSTACLE_WIDTH, PLAYER_SIZE, WHITE);

    display.display();
  }
}
