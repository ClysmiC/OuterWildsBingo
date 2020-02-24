function assert(value) {
	if (!value) {
		debugger;
		alert("Assertion failed");
	}
}

function cellsFromHeaderId(headerId) {
	let cells = [];

	if (headerId === "tlbr") {
		cells.push(document.getElementById("0_0"));
		cells.push(document.getElementById("1_1"));
		cells.push(document.getElementById("2_2"));
		cells.push(document.getElementById("3_3"));
		cells.push(document.getElementById("4_4"));
	}
	else if (headerId === "bltr") {
		cells.push(document.getElementById("4_0"));
		cells.push(document.getElementById("3_1"));
		cells.push(document.getElementById("2_2"));
		cells.push(document.getElementById("1_3"));
		cells.push(document.getElementById("0_4"));
	}
	else if (headerId.startsWith("col")) {
		let col = headerId.slice(-1);
		col = parseInt(col) - 1;

		for (let i = 0; i < 5; i++) {
			cells.push(document.getElementById(i + "_" + col));
		}
	}
	else if (headerId.startsWith("row")) {
		let row = headerId.slice(-1);
		row = parseInt(row) - 1;

		for (let i = 0; i < 5; i++) {
			cells.push(document.getElementById(row + "_" + i));
		}
	}
	else {
		assert(false);
	}

	return cells;
}

function shorthandLookup(strShort) {
	for (let i = 0; i < manifest.shorthands.length; i++) {
		if (manifest.shorthands[i].short === strShort) {
			return manifest.shorthands[i].long;
		}
	}

	return "";
}

function htmlFromStringWithShorthands(str) {
	let iLBrack = -1;
	let iRBrack = -1;
	let result = "";

	for (let i = 0; i < str.length; i++) {
		let c = str[i];

		if (c === "[") {
			iLBrack = i;
		}
		else if (c === "]") {
			iRBrack = i;

			if (iLBrack !== -1) {
				let strShort = str.substring(iLBrack + 1, iRBrack);
				result += "<span class='ttCrumb'>" + strShort + "<span class='ttText'>" + shorthandLookup(strShort) + "</span></span>";
				iLBrack = -1;
				iRBrack = -1;
			}
		}
		else if (iLBrack === -1) {
			result += c;
		}
	}

	return result;
}

function htmlFromGoal(goal) {
	let result = htmlFromStringWithShorthands(goal.text);

	if (goal.alt !== "") {
		result += " <span class='ttCrumb'>[?]<span class='ttText'>" + htmlFromStringWithShorthands(goal.alt) + "</span></span>";
	}

	return result;
}

function initLayoutAndHandlers() {

	// Get random seed from URL or generate one

	if (window.location.href.includes("?seed=")) {
		let seedIndex = window.location.href.indexOf("?seed=");
		seedIndex += "?seed=".length;

		seedRng(parseInt(window.location.href.slice(seedIndex)));
	}
	else {
		let generatedSeed = Math.floor(Math.random() * 1000000000);

		// NOTE (this performs a redirect)

		window.location.replace(window.location.href + "?seed=" + generatedSeed);
		return;
	}

	// Set cell size

	{
		let dimension = 0.7 * Math.min(window.innerWidth, window.innerHeight);
		dimension = Math.max(400, dimension);
		dimension = Math.min(800, dimension);

		let cellDimension = dimension / 5.0;

		let cells = document.getElementsByTagName("td");
		for (let i = 0; i < cells.length; i++) {
			cells[i].style.width = cellDimension + "px";
			cells[i].style.height = cellDimension + "px";
		}
	}

	// Resize right panel

	let panelCard = document.getElementById("panelCard");
	let panelRight = document.getElementById("panelRight");

	panelRight.style.height = panelCard.offsetHeight + 0.5 * (window.innerHeight - panelCard.offsetHeight) + "px";
	panelRight.style.width = (window.innerWidth - panelCard.offsetWidth - 50) + "px";

	// Initialize new card button

	document.getElementById("btnNewCard").addEventListener("click", function () {
		let locationSubstring = window.location.href;
		if (window.location.href.includes("?seed=")) {
			locationSubstring = locationSubstring.substring(0, window.location.href.indexOf("?seed="));
		}
		
		let generatedSeed = Math.floor(Math.random() * 1000000000);

		// NOTE (this performs a redirect)

		window.location.replace(locationSubstring + "?seed=" + generatedSeed);
		return;
	});

	// Initialize row/column headers

	let headerSelected = null;
	let headers = document.getElementsByTagName("th");
	for (let i = 0; i < headers.length; i++) {
		let cells = cellsFromHeaderId(headers[i].id);

		headers[i].addEventListener("mouseenter", function () {
			for (let i = 0; i < cells.length; i++) {
				cells[i].classList.add("fakeHover");
			}
		});

		headers[i].addEventListener("mouseout", function () {
			for (let i = 0; i < cells.length; i++) {
				cells[i].classList.remove("fakeHover");
				cells[i].classList.remove("preselected");
			}
		});

		headers[i].addEventListener("mousedown", function () {
			for (let i = 0; i < cells.length; i++) {
				cells[i].classList.add("preselected");
			}
		});

		headers[i].addEventListener("mouseup", function () {
			for (let i = 0; i < cells.length; i++) {
				cells[i].classList.remove("preselected");
			}
		});

		headers[i].addEventListener("click", function () {
			if (headerSelected !== null) {

				assert(headerSelected.classList.contains("selected"));
				headerSelected.classList.remove("selected");

				let cellsPrev = cellsFromHeaderId(headerSelected.id);
				for (let i = 0; i < cellsPrev.length; i++) {
					cellsPrev[i].classList.remove("selected");
				}
			}

			if (headerSelected === this) {
				headerSelected = null;
			}
			else {
				headerSelected = this;

				assert(!headerSelected.classList.contains("selected"));
				headerSelected.classList.add("selected");

				for (let i = 0; i < cells.length; i++) {
					cells[i].classList.add("selected");
				}
			}
		});
	}

	// Init click listener for goals

	let goalCells = [];
	for (let i = 0; i < 5; i++) {
		goalCells.push([]);

		for (let j = 0; j < 5; j++) {
			let cell = document.getElementById(i + "_" + j);
			cell.addEventListener("click", function () {
				if (this.classList.contains("greenCell")) {
					this.classList.add("redCell");
					this.classList.remove("greenCell");
				}
				else if (this.classList.contains("redCell")) {
					this.classList.remove("redCell");
				}
				else {
					this.classList.add("greenCell");
				}
			});
		}
	}

	// Make visible

	document.getElementById("panelMain").style.visibility = "visible";
}

function initGoals() {

	// Init extra fields on goals

	for (let i = 0; i < manifest.goals.length; i++) {
		manifest.goals[i].isInBoard = false;
	}

	// Shuffle goals in manifest

	for (let i = 0; i < manifest.goals.length; i++) {
		let iNew = i + rngInt(manifest.goals.length - i);

		var tmp = manifest.goals[i];
		manifest.goals[i] = manifest.goals[iNew];
		manifest.goals[iNew] = tmp;
	}

	let iGoalNext = 0;
	function nextGoal() {
		while (manifest.goals[iGoalNext].isInBoard) {
			iGoalNext += 1;
			iGoalNext %= manifest.goals.length;
		}

		var result = manifest.goals[iGoalNext];
		iGoalNext += 1;
		iGoalNext %= manifest.goals.length;
		return result;
	}

	function setGoal(row, col, goal) {
		let cell = document.getElementById(row + "_" + col);

		let oldGoal = board[row][col];
		if (oldGoal !== null) {
			oldGoal.isInBoard = false;
		}

		cell.innerHTML = htmlFromGoal(goal);
		// cell.prop("title", goal.tooltip);

		board[row][col] = goal;
		goal.isInBoard = true;
	}

	// Create empty null array because I don't really know how JavaScript arrays work so
	//	I'd like to explicitly set its capacity.

	let board = [];
	for (let i = 0; i < 5; i++) {
		board.push([]);
		for (let j = 0; j < 5; j++) {
			board[i].push(null);
		}
	}

	// Pick random goals for each cell

	for (let i = 0; i < 5; i++) {
		for (let j = 0; j < 5; j++) {
			setGoal(i, j, nextGoal());
		}
	}
}

initLayoutAndHandlers();
initGoals();