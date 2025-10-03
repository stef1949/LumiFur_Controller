# Contributing to LumiFur Controller 🎉

Thank you for your interest in contributing to the LumiFur Controller project! We appreciate your time and effort in helping make this protogen LED matrix controller even better. Every contribution, whether it's a bug fix, feature enhancement, documentation improvement, or even just reporting an issue, helps improve the project for everyone in the community.

## 🐛 Reporting Bugs

If you encounter a bug or unexpected behavior, please help us fix it by creating a detailed bug report:

1. **Search existing issues** first to see if the bug has already been reported
2. **Create a new issue** using our [Bug Report template](https://github.com/stef1949/LumiFur_Controller/issues/new?assignees=&labels=bug&template=bug_report.md&title=%5BBUG%5D)
3. **Provide detailed information** including:
   - Clear description of the bug
   - Steps to reproduce the issue
   - Expected vs actual behavior
   - Hardware setup (ESP32 board, LED matrix configuration)
   - Software environment (PlatformIO version, library versions)
   - Screenshots or videos if applicable

## 💡 Requesting Features

Have an idea for a new feature or enhancement? We'd love to hear about it!

1. **Search existing issues** to see if the feature has already been requested
2. **Create a new issue** using our [Feature Request template](https://github.com/stef1949/LumiFur_Controller/issues/new?assignees=&labels=enhancement&template=feature_request.md&title=%5BFEATURE%5D)
3. **Describe your idea** including:
   - What problem the feature would solve
   - How you envision it working
   - Any additional context or examples

## 🔧 Contributing Code

Ready to contribute code? Awesome! Here's how to get started:

### 1. Fork the Repository

1. Navigate to the [LumiFur_Controller repository](https://github.com/stef1949/LumiFur_Controller)
2. Click the "Fork" button in the top-right corner
3. This creates your own copy of the repository under your GitHub account

### 2. Clone Your Fork

```bash
git clone https://github.com/YOUR_USERNAME/LumiFur_Controller.git
cd LumiFur_Controller
```

### 3. Set Up Development Environment

Ensure you have the required tools installed:

- **PlatformIO** (with VSCode extension recommended)
- **Git**
- **GitHub Copilot** (recommended for enhanced development experience)

The project will automatically handle library dependencies defined in `platformio.ini`.

#### GitHub Copilot Integration 🤖

This project includes comprehensive GitHub Copilot instructions to help you develop more efficiently:

- **Review Instructions**: Check `.github/copilot-instructions.md` for detailed project context
- **Usage Guide**: See `docs/COPILOT_USAGE.md` for development workflow tips
- **Chat Instructions**: Use `.github/copilot-chat-instructions.md` for focused assistance

Copilot understands our embedded C++ patterns, ESP32 constraints, and PlatformIO build system, making it an excellent tool for both new contributors and experienced developers.

### 4. Create a Descriptive Branch

Create a new branch for your changes with a descriptive name:

```bash
# For bug fixes
git checkout -b fix/issue-description

# For new features  
git checkout -b feature/feature-description

# For documentation updates
git checkout -b docs/update-description

# Examples:
git checkout -b fix/bluetooth-connection-timeout
git checkout -b feature/new-facial-expression
git checkout -b docs/update-installation-guide
```

### 5. Run the Test Suite

Before making changes, ensure all tests pass:

```bash
# Run unit tests for native environment
pio test -e native

# Run tests with coverage
pio test -e native2

# For hardware-specific tests (if available)
pio test -e adafruit_matrixportal_esp32s3
```

If you encounter test failures not related to your changes, please report them as separate issues.

### 6. Implement Your Changes

- **Write clean, readable code** that follows the existing code style
- **Add comments** where necessary to explain complex logic
- **Test your changes thoroughly** on actual hardware when possible
- **Update documentation** if your changes affect user-facing functionality
- **Add or update tests** to cover your new code

#### Code Style Guidelines

- Follow existing naming conventions
- Use consistent indentation (spaces vs tabs as per existing files)
- Keep functions focused and reasonably sized
- Add meaningful comments for complex hardware interactions

### 7. Test Your Changes

Before submitting your pull request:

```bash
# Build for your target environment
pio run -e adafruit_matrixportal_esp32s3

# Run tests
pio test

# Test on actual hardware if possible
pio run -e adafruit_matrixportal_esp32s3 -t upload
```

### 8. Commit Your Changes

Write clear, descriptive commit messages:

```bash
git add .
git commit -m "Fix: Resolve Bluetooth connection timeout issue

- Increase connection timeout from 5s to 15s
- Add retry logic for failed connections
- Update error messaging for connection failures

Fixes #123"
```

### 9. Keep Your Branch Updated

Before submitting your pull request, make sure your branch is up to date:

```bash
# Add the original repository as upstream
git remote add upstream https://github.com/stef1949/LumiFur_Controller.git

# Fetch latest changes
git fetch upstream

# Rebase your branch onto the latest main
git rebase upstream/main
```

If there are conflicts, resolve them and continue the rebase:

```bash
git add .
git rebase --continue
```

### 10. Submit a Pull Request

1. **Push your branch** to your fork:
   ```bash
   git push origin your-branch-name
   ```

2. **Create a pull request** from your fork to the main repository:
   - Go to your fork on GitHub
   - Click "New Pull Request"
   - Select your branch and the main repository's `main` branch
   - Fill out the pull request template with:
     - Clear description of changes
     - Link to related issues
     - Testing performed
     - Screenshots/videos for UI changes

3. **Respond to feedback** and make requested changes if needed

### 11. Keep Your Pull Request Updated

If the main branch receives updates while your PR is open:

```bash
# Fetch and rebase onto latest main
git fetch upstream
git rebase upstream/main

# Force push to update your PR (safe for PRs)
git push --force-with-lease origin your-branch-name
```

## 🧪 Testing

This project uses multiple testing frameworks:

- **Unity** for embedded unit testing
- **GoogleTest** for native testing
- **Coverage** analysis for code quality

When adding new features:
- Write unit tests for core functionality
- Test on actual hardware when possible
- Include edge case testing

## 📋 Pull Request Guidelines

- **One feature per PR** - keep changes focused and atomic
- **Update documentation** for user-facing changes
- **Add tests** for new functionality
- **Follow the existing code style**
- **Write descriptive commit messages**
- **Reference related issues** using keywords like "Fixes #123"

## 📞 Getting Help

Need help or have questions?

- **Check existing issues** and discussions
- **Create a new issue** with the "question" label
- **Join community discussions** in the project's issue tracker

## 📜 Code of Conduct

This project follows our [Code of Conduct](CODE_OF_CONDUCT.md). By participating, you agree to uphold this code. Please report unacceptable behavior to the project maintainers.

## 🙏 Recognition

All contributors will be recognized in our project documentation. Thank you for helping make LumiFur Controller better for the entire protogen community!

---

*Happy contributing! 🎭✨*
Have an idea for a new feature or enhancement? We'd love to hear about it!

1. **Search existing issues** to see if the feature has already been requested
2. **Create a new issue** using our [Feature Request template](https://github.com/stef1949/LumiFur_Controller/issues/new?assignees=&labels=enhancement&template=feature_request.md&title=%5BFEATURE%5D)
3. **Describe your idea** including:
   - What problem the feature would solve
   - How you envision it working
   - Any additional context or examples

## 🔧 Contributing Code

Ready to contribute code? Awesome! Here's how to get started:

### 1. Fork the Repository

1. Navigate to the [LumiFur_Controller repository](https://github.com/stef1949/LumiFur_Controller)
2. Click the "Fork" button in the top-right corner
3. This creates your own copy of the repository under your GitHub account

### 2. Clone Your Fork

```bash
git clone https://github.com/YOUR_USERNAME/LumiFur_Controller.git
cd LumiFur_Controller
```

### 3. Set Up Development Environment

Ensure you have the required tools installed:

- **PlatformIO** (with VSCode extension recommended)
- **Git**

The project will automatically handle library dependencies defined in `platformio.ini`.

### 4. Create a Descriptive Branch

Create a new branch for your changes with a descriptive name:

```bash
# For bug fixes
git checkout -b fix/issue-description

# For new features  
git checkout -b feature/feature-description

# For documentation updates
git checkout -b docs/update-description

# Examples:
git checkout -b fix/bluetooth-connection-timeout
git checkout -b feature/new-facial-expression
git checkout -b docs/update-installation-guide
```

### 5. Run the Test Suite

Before making changes, ensure all tests pass:

```bash
# Run unit tests for native environment
pio test -e native

# Run tests with coverage
pio test -e native2

# For hardware-specific tests (if available)
pio test -e adafruit_matrixportal_esp32s3
```

If you encounter test failures not related to your changes, please report them as separate issues.

### 6. Implement Your Changes

- **Write clean, readable code** that follows the existing code style
- **Add comments** where necessary to explain complex logic
- **Test your changes thoroughly** on actual hardware when possible
- **Update documentation** if your changes affect user-facing functionality
- **Add or update tests** to cover your new code

#### Code Style Guidelines

- Follow existing naming conventions
- Use consistent indentation (spaces vs tabs as per existing files)
- Keep functions focused and reasonably sized
- Add meaningful comments for complex hardware interactions

### 7. Test Your Changes

Before submitting your pull request:

```bash
# Build for your target environment
pio run -e adafruit_matrixportal_esp32s3

# Run tests
pio test

# Test on actual hardware if possible
pio run -e adafruit_matrixportal_esp32s3 -t upload
```

### 8. Commit Your Changes

Write clear, descriptive commit messages:

```bash
git add .
git commit -m "Fix: Resolve Bluetooth connection timeout issue

- Increase connection timeout from 5s to 15s
- Add retry logic for failed connections
- Update error messaging for connection failures

Fixes #123"
```

### 9. Keep Your Branch Updated

Before submitting your pull request, make sure your branch is up to date:

```bash
# Add the original repository as upstream
git remote add upstream https://github.com/stef1949/LumiFur_Controller.git

# Fetch latest changes
git fetch upstream

# Rebase your branch onto the latest main
git rebase upstream/main
```

If there are conflicts, resolve them and continue the rebase:

```bash
git add .
git rebase --continue
```

### 10. Submit a Pull Request

1. **Push your branch** to your fork:

   ```bash
   git push origin your-branch-name
   ```

2. **Create a pull request** from your fork to the main repository:
   - Go to your fork on GitHub
   - Click "New Pull Request"
   - Select your branch and the main repository's `main` branch
   - Fill out the pull request template with:
     - Clear description of changes
     - Link to related issues
     - Testing performed
     - Screenshots/videos for UI changes

3. **Respond to feedback** and make requested changes if needed

### 11. Keep Your Pull Request Updated

If the main branch receives updates while your PR is open:

```bash
# Fetch and rebase onto latest main
git fetch upstream
git rebase upstream/main

# Force push to update your PR (safe for PRs)
git push --force-with-lease origin your-branch-name
```

## 🧪 Testing

This project uses multiple testing frameworks:

- **Unity** for embedded unit testing
- **GoogleTest** for native testing
- **Coverage** analysis for code quality

When adding new features:

- Write unit tests for core functionality
- Test on actual hardware when possible
- Include edge case testing

## 📋 Pull Request Guidelines

- **One feature per PR** - keep changes focused and atomic
- **Update documentation** for user-facing changes
- **Add tests** for new functionality
- **Follow the existing code style**
- **Write descriptive commit messages**
- **Reference related issues** using keywords like "Fixes #123"

## 📞 Getting Help

Need help or have questions?

- **Check existing issues** and discussions
- **Create a new issue** with the "question" label
- **Join community discussions** in the project's issue tracker

## 📜 Code of Conduct

This project follows our [Code of Conduct](CODE_OF_CONDUCT.md). By participating, you agree to uphold this code. Please report unacceptable behavior to the project maintainers.

## 🙏 Recognition

All contributors will be recognized in our project documentation. Thank you for helping make LumiFur Controller better for the entire protogen community!

---

Happy contributing! 🎭✨