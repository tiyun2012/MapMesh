import sys
from PySide6.QtWidgets import QApplication
from ui_main import ProfessionalClosestPointApp


if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = ProfessionalClosestPointApp()
    window.show()
    sys.exit(app.exec())